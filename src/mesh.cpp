#include "mesh.h"

#include "gl_loader.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {
constexpr float kPi = 3.14159265358979f;

glm::vec3 FaceNormal(glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 n = glm::cross(b - a, c - a);
    float len = glm::length(n);
    return len > 1e-12f ? n / len : glm::vec3(0, 1, 0);
}
} // namespace

// ---------------------------------------------------------------------------
// MeshBuilder
// ---------------------------------------------------------------------------
void MeshBuilder::addTriangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color)
{
    glm::vec3 n = FaceNormal(a, b, c);
    auto base = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back({a, n, color});
    vertices.push_back({b, n, color});
    vertices.push_back({c, n, color});
    indices.insert(indices.end(), {base, base + 1, base + 2});
}

void MeshBuilder::addQuad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 color)
{
    glm::vec3 n = FaceNormal(a, b, c);
    auto base = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back({a, n, color});
    vertices.push_back({b, n, color});
    vertices.push_back({c, n, color});
    vertices.push_back({d, n, color});
    indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

namespace {
// Quad whose winding is chosen so the normal points away from `inside`.
void QuadOutward(MeshBuilder& mb, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 inside,
                 glm::vec3 color)
{
    glm::vec3 n = glm::cross(b - a, c - a);
    glm::vec3 centre = (a + b + c + d) * 0.25f;
    if (glm::dot(n, centre - inside) < 0.0f) mb.addQuad(d, c, b, a, color);
    else                                      mb.addQuad(a, b, c, d, color);
}
} // namespace

void MeshBuilder::addBox(const glm::mat4& xf, glm::vec3 color)
{
    glm::vec3 p[8];
    for (int i = 0; i < 8; ++i) {
        glm::vec4 local((i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f, 1.0f);
        p[i] = glm::vec3(xf * local);
    }
    glm::vec3 c = glm::vec3(xf * glm::vec4(0, 0, 0, 1));
    QuadOutward(*this, p[0], p[1], p[3], p[2], c, color); // -z
    QuadOutward(*this, p[4], p[5], p[7], p[6], c, color); // +z
    QuadOutward(*this, p[0], p[1], p[5], p[4], c, color); // -y
    QuadOutward(*this, p[2], p[3], p[7], p[6], c, color); // +y
    QuadOutward(*this, p[0], p[2], p[6], p[4], c, color); // -x
    QuadOutward(*this, p[1], p[3], p[7], p[5], c, color); // +x
}

void MeshBuilder::addBox(glm::vec3 center, glm::vec3 size, glm::vec3 color)
{
    glm::mat4 xf(1.0f);
    xf[0][0] = size.x;
    xf[1][1] = size.y;
    xf[2][2] = size.z;
    xf[3] = glm::vec4(center, 1.0f);
    addBox(xf, color);
}

void MeshBuilder::addBeam(glm::vec3 a, glm::vec3 b, float thickness, glm::vec3 color)
{
    glm::vec3 d = b - a;
    float len = glm::length(d);
    if (len < 1e-6f) return;
    glm::vec3 dir = d / len;
    glm::vec3 up = std::abs(dir.y) < 0.95f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 side = glm::normalize(glm::cross(dir, up));
    glm::vec3 up2 = glm::cross(side, dir);
    glm::mat4 xf(1.0f);
    xf[0] = glm::vec4(side * thickness, 0);
    xf[1] = glm::vec4(up2 * thickness, 0);
    xf[2] = glm::vec4(d, 0);
    xf[3] = glm::vec4((a + b) * 0.5f, 1);
    addBox(xf, color);
}

void MeshBuilder::addLoft(const std::vector<Section>& s, glm::vec3 color, glm::vec3 capColor)
{
    auto corners = [](const Section& q, glm::vec3 out[4]) {
        out[0] = {q.x - q.w * 0.5f, q.y - q.h * 0.5f, q.z};
        out[1] = {q.x + q.w * 0.5f, q.y - q.h * 0.5f, q.z};
        out[2] = {q.x + q.w * 0.5f, q.y + q.h * 0.5f, q.z};
        out[3] = {q.x - q.w * 0.5f, q.y + q.h * 0.5f, q.z};
    };
    for (size_t i = 0; i + 1 < s.size(); ++i) {
        glm::vec3 a[4], b[4];
        corners(s[i], a);
        corners(s[i + 1], b);
        glm::vec3 inside((s[i].x + s[i + 1].x) * 0.5f, (s[i].y + s[i + 1].y) * 0.5f, (s[i].z + s[i + 1].z) * 0.5f);
        for (int k = 0; k < 4; ++k) {
            int n = (k + 1) % 4;
            QuadOutward(*this, a[k], a[n], b[n], b[k], inside, color);
        }
    }
    if (s.size() >= 2) {
        glm::vec3 c[4];
        corners(s.front(), c);
        QuadOutward(*this, c[0], c[1], c[2], c[3], {s[1].x, s[1].y, s[1].z}, capColor);
        corners(s.back(), c);
        const Section& p = s[s.size() - 2];
        QuadOutward(*this, c[0], c[1], c[2], c[3], {p.x, p.y, p.z}, capColor);
    }
}

void MeshBuilder::addCylinderX(float x0, float x1, float r, int seg, glm::vec3 colA, glm::vec3 colB, int stripe)
{
    for (int i = 0; i < seg; ++i) {
        float a0 = 2 * kPi * i / seg, a1 = 2 * kPi * (i + 1) / seg;
        glm::vec3 n0(0, std::cos(a0), std::sin(a0)), n1(0, std::cos(a1), std::sin(a1));
        glm::vec3 col = (stripe > 0 && (i / stripe) % 2) ? colB : colA;
        auto base = static_cast<std::uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(x0, 0, 0) + n0 * r, n0, col});
        vertices.push_back({glm::vec3(x1, 0, 0) + n0 * r, n0, col});
        vertices.push_back({glm::vec3(x1, 0, 0) + n1 * r, n1, col});
        vertices.push_back({glm::vec3(x0, 0, 0) + n1 * r, n1, col});
        indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
}

void MeshBuilder::addAnnulusX(float x, float r0, float r1, int seg, bool facePositive, glm::vec3 colA,
                              glm::vec3 colB, int stripe)
{
    glm::vec3 n(facePositive ? 1.0f : -1.0f, 0, 0);
    for (int i = 0; i < seg; ++i) {
        float a0 = 2 * kPi * i / seg, a1 = 2 * kPi * (i + 1) / seg;
        glm::vec3 d0(0, std::cos(a0), std::sin(a0)), d1(0, std::cos(a1), std::sin(a1));
        glm::vec3 col = (stripe > 0 && (i / stripe) % 2) ? colB : colA;
        glm::vec3 c(x, 0, 0);
        auto base = static_cast<std::uint32_t>(vertices.size());
        vertices.push_back({c + d0 * r0, n, col});
        vertices.push_back({c + d0 * r1, n, col});
        vertices.push_back({c + d1 * r1, n, col});
        vertices.push_back({c + d1 * r0, n, col});
        indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
}

void MeshBuilder::addTorusZ(glm::vec3 center, float major, float minor, int segU, int segV, glm::vec3 color)
{
    auto base = static_cast<std::uint32_t>(vertices.size());
    for (int i = 0; i <= segU; ++i) {
        float u = 2 * kPi * i / segU;
        glm::vec3 ring(std::cos(u), std::sin(u), 0);
        for (int j = 0; j <= segV; ++j) {
            float v = 2 * kPi * j / segV;
            glm::vec3 n = ring * std::cos(v) + glm::vec3(0, 0, std::sin(v));
            vertices.push_back({center + ring * major + n * minor, n, color});
        }
    }
    for (int i = 0; i < segU; ++i)
        for (int j = 0; j < segV; ++j) {
            std::uint32_t a = base + i * (segV + 1) + j, b = a + segV + 1;
            indices.insert(indices.end(), {a, b, b + 1, a, b + 1, a + 1});
        }
}

void MeshBuilder::addSphere(glm::vec3 center, float radius, int seg, glm::vec3 color)
{
    auto base = static_cast<std::uint32_t>(vertices.size());
    const int rings = seg / 2;
    for (int i = 0; i <= rings; ++i) {
        float th = kPi * i / rings;
        for (int j = 0; j <= seg; ++j) {
            float ph = 2 * kPi * j / seg;
            glm::vec3 n(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
            vertices.push_back({center + n * radius, n, color});
        }
    }
    for (int i = 0; i < rings; ++i)
        for (int j = 0; j < seg; ++j) {
            std::uint32_t a = base + i * (seg + 1) + j, b = a + seg + 1;
            indices.insert(indices.end(), {a, b, b + 1, a, b + 1, a + 1});
        }
}

void MeshBuilder::append(const MeshBuilder& other, const glm::mat4& xf)
{
    glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(xf)));
    auto base = static_cast<std::uint32_t>(vertices.size());
    for (const Vertex& v : other.vertices)
        vertices.push_back({glm::vec3(xf * glm::vec4(v.pos, 1)), glm::normalize(nm * v.normal), v.color});
    for (std::uint32_t i : other.indices) indices.push_back(base + i);
}

// ---------------------------------------------------------------------------
// Mesh (GPU)
// ---------------------------------------------------------------------------
Mesh::~Mesh()
{
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void Mesh::upload(const MeshBuilder& b)
{
    if (!vao_) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);
    }
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(b.vertices.size() * sizeof(Vertex)), b.vertices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(b.indices.size() * sizeof(std::uint32_t)),
                 b.indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));
    glBindVertexArray(0);
    count_ = static_cast<int>(b.indices.size());
}

void Mesh::draw() const
{
    if (!count_) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// OBJ loader
// ---------------------------------------------------------------------------
namespace {

std::string DirectoryOf(const std::string& path)
{
    size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? std::string() : path.substr(0, p + 1);
}

void LoadMtl(const std::string& path, std::unordered_map<std::string, glm::vec3>& colors)
{
    std::ifstream in(path);
    std::string line, current;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "newmtl") {
            ss >> current;
            colors[current] = glm::vec3(0.8f);
        } else if (tag == "Kd" && !current.empty()) {
            glm::vec3 c;
            ss >> c.x >> c.y >> c.z;
            colors[current] = c;
        }
    }
}

} // namespace

bool LoadObj(const std::string& path, MeshBuilder& out, glm::vec3 defaultColor)
{
    std::ifstream in(path);
    if (!in) return false;

    std::vector<glm::vec3> positions, normals;
    std::unordered_map<std::string, glm::vec3> materials;
    glm::vec3 color = defaultColor;
    const size_t startIndexCount = out.indices.size();

    auto resolve = [](int idx, size_t count) -> int {
        return idx > 0 ? idx - 1 : static_cast<int>(count) + idx;
    };

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") {
            glm::vec3 p;
            ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (tag == "vn") {
            glm::vec3 n;
            ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (tag == "mtllib") {
            std::string file;
            std::getline(ss >> std::ws, file);
            LoadMtl(DirectoryOf(path) + file, materials);
        } else if (tag == "usemtl") {
            std::string name;
            ss >> name;
            auto it = materials.find(name);
            color = it != materials.end() ? it->second : defaultColor;
        } else if (tag == "f") {
            struct Corner { int v, n; };
            std::vector<Corner> face;
            std::string tok;
            while (ss >> tok) {
                Corner c{-1, -1};
                int v = std::atoi(tok.c_str());
                c.v = resolve(v, positions.size());
                size_t s1 = tok.find('/');
                if (s1 != std::string::npos) {
                    size_t s2 = tok.find('/', s1 + 1);
                    if (s2 != std::string::npos && s2 + 1 < tok.size())
                        c.n = resolve(std::atoi(tok.c_str() + s2 + 1), normals.size());
                }
                if (c.v < 0 || c.v >= static_cast<int>(positions.size())) { face.clear(); break; }
                if (c.n >= static_cast<int>(normals.size())) c.n = -1;
                face.push_back(c);
            }
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                const Corner tri[3] = {face[0], face[i], face[i + 1]};
                glm::vec3 fn = FaceNormal(positions[tri[0].v], positions[tri[1].v], positions[tri[2].v]);
                auto base = static_cast<std::uint32_t>(out.vertices.size());
                for (const Corner& c : tri)
                    out.vertices.push_back({positions[c.v], c.n >= 0 ? glm::normalize(normals[c.n]) : fn, color});
                out.indices.insert(out.indices.end(), {base, base + 1, base + 2});
            }
        }
    }
    return out.indices.size() > startIndexCount;
}
