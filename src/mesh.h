// GPU meshes, a small procedural geometry builder and a Wavefront OBJ loader.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;
};

class MeshBuilder {
public:
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    // Flat-shaded primitives
    void addTriangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color);
    void addQuad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 color);
    // Unit cube (-0.5..0.5) transformed by `xf`.
    void addBox(const glm::mat4& xf, glm::vec3 color);
    void addBox(glm::vec3 center, glm::vec3 size, glm::vec3 color);
    // Square-section beam from a to b (suspension arms, hoops, ...).
    void addBeam(glm::vec3 a, glm::vec3 b, float thickness, glm::vec3 color);

    // Rectangular cross-section at z, centred at (x, y), used by addLoft.
    struct Section { float z, x, y, w, h; };
    // Connects consecutive sections into a closed body with end caps.
    void addLoft(const std::vector<Section>& sections, glm::vec3 color, glm::vec3 capColor);

    // Smooth-shaded primitives. Cylinders/annuli are built around the X axis.
    void addCylinderX(float x0, float x1, float radius, int segments, glm::vec3 colorA, glm::vec3 colorB, int stripe);
    void addAnnulusX(float x, float r0, float r1, int segments, bool facePositive, glm::vec3 colorA,
                     glm::vec3 colorB, int stripe);
    // Torus in the XY plane (normal along Z).
    void addTorusZ(glm::vec3 center, float major, float minor, int segU, int segV, glm::vec3 color);
    void addSphere(glm::vec3 center, float radius, int segments, glm::vec3 color);

    void append(const MeshBuilder& other, const glm::mat4& xf);
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void upload(const MeshBuilder& builder);
    void draw() const;
    bool valid() const { return count_ > 0; }

private:
    unsigned vao_ = 0, vbo_ = 0, ebo_ = 0;
    int count_ = 0;
};

// Loads an OBJ (with optional .mtl diffuse colours) into `out`.
// Returns false if the file does not exist or has no faces.
bool LoadObj(const std::string& path, MeshBuilder& out, glm::vec3 defaultColor = glm::vec3(0.8f));
