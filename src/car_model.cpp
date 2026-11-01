#include "car_model.h"

#include "gl_loader.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include <glm/gtc/type_ptr.hpp>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>

namespace {

const char* const kPartNames[kCarPartCount] = {"body",       "wheel_FL",   "wheel_FR",      "wheel_RL",
                                               "wheel_RR",   "upright_FL", "upright_FR",    "steering_wheel"};

using SurfaceMap = std::map<std::pair<int, int>, MeshBuilder>;   // key: part, material

struct Bounds {
    glm::vec3 lo = glm::vec3(FLT_MAX), hi = glm::vec3(-FLT_MAX);
    void add(glm::vec3 p) { lo = glm::min(lo, p); hi = glm::max(hi, p); }
    bool valid() const { return lo.x <= hi.x; }
    glm::vec3 center() const { return (lo + hi) * 0.5f; }
    bool contains(glm::vec3 p) const
    {
        return glm::all(glm::greaterThanEqual(p, lo)) && glm::all(glm::lessThanEqual(p, hi));
    }
};

glm::vec3 SafeNormalize(glm::vec3 v)
{
    const float len = glm::length(v);
    return len > 1e-12f ? v / len : glm::vec3(0.0f);
}

// Appends a triangle primitive, baked into car space so parts only need a rotation at draw time.
void AppendPrimitive(const cgltf_primitive& prim, const glm::mat4& world, MeshBuilder& out)
{
    const cgltf_accessor* position = cgltf_find_accessor(&prim, cgltf_attribute_type_position, 0);
    if (prim.type != cgltf_primitive_type_triangles || !position) return;

    const size_t count = position->count;
    auto read = [&](cgltf_attribute_type type, int set, size_t components) {
        std::vector<float> values;
        const cgltf_accessor* a = cgltf_find_accessor(&prim, type, set);
        if (a && a->count == count && cgltf_num_components(a->type) == components) {
            values.resize(count * components);
            if (cgltf_accessor_unpack_floats(a, values.data(), values.size()) != values.size()) values.clear();
        }
        return values;
    };
    const std::vector<float> positions = read(cgltf_attribute_type_position, 0, 3);
    const std::vector<float> normals = read(cgltf_attribute_type_normal, 0, 3);
    const std::vector<float> tangents = read(cgltf_attribute_type_tangent, 0, 4);
    const std::vector<float> uv0 = read(cgltf_attribute_type_texcoord, 0, 2);
    const std::vector<float> uv1 = read(cgltf_attribute_type_texcoord, 1, 2);
    const std::vector<float> color3 = read(cgltf_attribute_type_color, 0, 3);
    const std::vector<float> color4 = read(cgltf_attribute_type_color, 0, 4);
    if (positions.empty()) return;

    const glm::mat3 linear(world);
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(linear));
    const float handedness = glm::determinant(linear) < 0.0f ? -1.0f : 1.0f;   // mirrored objects
    const auto base = static_cast<std::uint32_t>(out.vertices.size());
    for (size_t i = 0; i < count; ++i) {
        // A zero normal tells the shader to shade the triangle flat.
        Vertex v{glm::vec3(world * glm::vec4(glm::make_vec3(&positions[i * 3]), 1.0f)), glm::vec3(0.0f), glm::vec3(1.0f)};
        if (!normals.empty()) v.normal = SafeNormalize(normalMatrix * glm::make_vec3(&normals[i * 3]));
        if (!tangents.empty())
            v.tangent = glm::vec4(SafeNormalize(linear * glm::make_vec3(&tangents[i * 4])), tangents[i * 4 + 3] * handedness);
        if (!uv0.empty()) v.uv0 = glm::make_vec2(&uv0[i * 2]);
        if (!uv1.empty()) v.uv1 = glm::make_vec2(&uv1[i * 2]);
        if (!color3.empty()) v.color = glm::make_vec3(&color3[i * 3]);
        if (!color4.empty()) v.color = glm::make_vec3(&color4[i * 4]);
        out.vertices.push_back(v);
    }
    if (prim.indices) {
        for (size_t i = 0; i < prim.indices->count; ++i)
            out.indices.push_back(base + static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices, i)));
    } else {
        for (size_t i = 0; i < count; ++i) out.indices.push_back(base + static_cast<std::uint32_t>(i));
    }
}

// Walks a node tree. Children inherit the part of the nearest named parent, so a part can
// be a single object or an Empty with several objects below it.
void CollectNode(const cgltf_data& data, const cgltf_node& node, int part, SurfaceMap& surfaces, glm::vec3* origins)
{
    float m[16];
    cgltf_node_transform_world(&node, m);
    const glm::mat4 world = glm::make_mat4(m);

    if (node.name) {
        for (int i = 0; i < kCarPartCount; ++i) {
            if (std::strcmp(node.name, kPartNames[i]) == 0) {
                part = i;
                origins[i] = glm::vec3(world[3]);
            }
        }
    }
    if (node.mesh) {
        for (size_t i = 0; i < node.mesh->primitives_count; ++i) {
            const cgltf_primitive& prim = node.mesh->primitives[i];
            // The material list ends with one extra entry for objects without a material.
            const int material = static_cast<int>(prim.material ? cgltf_material_index(&data, prim.material)
                                                                : data.materials_count);
            AppendPrimitive(prim, world, surfaces[{part, material}]);
        }
    }
    for (size_t i = 0; i < node.children_count; ++i) CollectNode(data, *node.children[i], part, surfaces, origins);
}

// Decodes and uploads each image once per sampler and colour space. Blender writes a separate
// glTF texture for every use of an image, so caching by texture would upload copies.
class TextureCache {
public:
    TextureCache(std::vector<unsigned>& owned, std::string& note) : owned_(owned), note_(note) {}

    MaterialMap map(const cgltf_texture_view& view, bool srgb)
    {
        MaterialMap out;
        if (!view.texture) return out;
        out.texture = texture(*view.texture, srgb);
        out.uvSet = view.texcoord;
        if (view.has_transform) {
            const cgltf_texture_transform& x = view.transform;
            if (x.has_texcoord) out.uvSet = x.texcoord;
            const float c = std::cos(x.rotation), s = std::sin(x.rotation);
            out.uvTransform = glm::mat3(c * x.scale[0], -s * x.scale[0], 0.0f,
                                        s * x.scale[1], c * x.scale[1], 0.0f,
                                        x.offset[0], x.offset[1], 1.0f);
        }
        if (out.uvSet > 1) {
            note_ += "A texture uses a third UV map, only the first two are supported\n";
            out.uvSet = 0;
        }
        return out;
    }

private:
    unsigned texture(const cgltf_texture& t, bool srgb)
    {
        unsigned& cached = cache_[Key(t.image, t.sampler, srgb)];
        if (cached) return cached;

        const cgltf_buffer_view* view = t.image ? t.image->buffer_view : nullptr;
        int w = 0, h = 0, components = 0;
        stbi_uc* pixels = view ? stbi_load_from_memory(cgltf_buffer_view_data(view), static_cast<int>(view->size), &w, &h,
                                                       &components, 4)
                               : nullptr;
        if (!pixels) {
            const char* name = t.image && t.image->name ? t.image->name : "(unnamed)";
            note_ += std::string("Texture ") + name + " could not be decoded, export it as PNG or JPEG\n";
            return 0;
        }

        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        const cgltf_sampler* s = t.sampler;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s ? s->wrap_s : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s ? s->wrap_t : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s && s->min_filter ? s->min_filter : GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s && s->mag_filter ? s->mag_filter : GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(pixels);

        owned_.push_back(id);
        cached = id;
        return id;
    }

    using Key = std::tuple<const cgltf_image*, const cgltf_sampler*, bool>;

    std::vector<unsigned>& owned_;
    std::string& note_;
    std::map<Key, unsigned> cache_;
};

ModelMaterial ToMaterial(const cgltf_material& m, TextureCache& textures)
{
    const cgltf_pbr_metallic_roughness& pbr = m.pbr_metallic_roughness;
    ModelMaterial out;
    out.baseColor = textures.map(pbr.base_color_texture, true);
    out.metallicRoughness = textures.map(pbr.metallic_roughness_texture, false);
    out.normal = textures.map(m.normal_texture, false);
    out.occlusion = textures.map(m.occlusion_texture, false);
    out.emissive = textures.map(m.emissive_texture, true);
    out.baseColorFactor = glm::make_vec4(pbr.base_color_factor);
    out.metallicFactor = pbr.metallic_factor;
    out.roughnessFactor = pbr.roughness_factor;
    out.emissiveFactor = glm::make_vec3(m.emissive_factor);
    if (m.has_emissive_strength) out.emissiveFactor *= m.emissive_strength.emissive_strength;
    out.normalScale = m.normal_texture.scale;
    out.occlusionStrength = m.occlusion_texture.scale;
    out.alphaMode = m.alpha_mode == cgltf_alpha_mode_mask    ? AlphaMode::Mask
                    : m.alpha_mode == cgltf_alpha_mode_blend ? AlphaMode::Blend
                                                             : AlphaMode::Opaque;
    out.alphaCutoff = m.alpha_cutoff;
    return out;
}

} // namespace

CarModel::~CarModel()
{
    if (!textures_.empty()) glDeleteTextures(static_cast<GLsizei>(textures_.size()), textures_.data());
    if (white_) glDeleteTextures(1, &white_);
}

bool CarModel::load(const std::string& path, std::string* note)
{
    note->clear();
    cgltf_options options{};
    cgltf_data* raw = nullptr;
    if (cgltf_parse_file(&options, path.c_str(), &raw) != cgltf_result_success) {
        *note = path + " could not be read as glTF\n";
        return false;
    }
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
    if (cgltf_load_buffers(&options, raw, path.c_str()) != cgltf_result_success ||
        cgltf_validate(raw) != cgltf_result_success) {
        *note = path + " is damaged or incomplete\n";
        return false;
    }
    for (size_t i = 0; i < raw->meshes_count; ++i) {
        for (size_t j = 0; j < raw->meshes[i].primitives_count; ++j) {
            if (raw->meshes[i].primitives[j].has_draco_mesh_compression) {
                *note = path + " uses Draco compression, export it again with Compression turned off\n";
                return false;
            }
        }
    }

    SurfaceMap builders;
    glm::vec3 origins[kCarPartCount] = {};
    for (size_t i = 0; i < raw->nodes_count; ++i)
        if (!raw->nodes[i].parent) CollectNode(*raw, raw->nodes[i], static_cast<int>(CarPart::Body), builders, origins);

    // Stand the car on the ground, centred under the orbit camera.
    Bounds all;
    for (const auto& entry : builders)
        for (const Vertex& v : entry.second.vertices) all.add(v.pos);
    if (!all.valid()) {
        *note = path + " contains no meshes\n";
        return false;
    }
    const glm::vec3 shift(-all.center().x, -all.lo.y, -all.center().z);
    for (glm::vec3& origin : origins) origin += shift;

    TextureCache textures(textures_, *note);
    materials_.clear();
    for (size_t i = 0; i < raw->materials_count; ++i) materials_.push_back(ToMaterial(raw->materials[i], textures));
    materials_.emplace_back();

    Bounds parts[kCarPartCount];
    for (auto& entry : builders) {
        MeshBuilder& builder = entry.second;
        if (builder.indices.empty()) continue;
        Bounds bounds;
        for (Vertex& v : builder.vertices) {
            v.pos += shift;
            bounds.add(v.pos);
            parts[entry.first.first].add(v.pos);
        }
        surfaces_.emplace_back();
        ModelSurface& surface = surfaces_.back();
        surface.part = static_cast<CarPart>(entry.first.first);
        surface.material = entry.first.second;
        surface.center = bounds.center();
        surface.mesh.upload(builder);
    }
    if (surfaces_.empty()) {
        *note = path + " contains no triangles\n";
        return false;
    }

    // Wheels are round, so the middle of their bounds is the axle whatever their origin is.
    // Uprights steer around the same point as their wheel, or the two would drift apart.
    for (int i = 0; i < kCarPartCount; ++i) {
        hasPart_[i] = parts[i].valid();
        pivots_[i] = hasPart_[i] ? parts[i].center() : glm::vec3(0.0f);
    }
    pivots_[static_cast<int>(CarPart::UprightFL)] = pivot(CarPart::WheelFL);
    pivots_[static_cast<int>(CarPart::UprightFR)] = pivot(CarPart::WheelFR);

    // The steering wheel is not symmetric, so its origin matters. An origin outside the part
    // means the transforms were applied in Blender and the origin was lost.
    const int sw = static_cast<int>(CarPart::SteeringWheel);
    if (hasPart_[sw]) {
        if (parts[sw].contains(origins[sw])) pivots_[sw] = origins[sw];
        else *note += "steering_wheel: origin is not on the part, using its centre\n";
    }

    const CarPart required[] = {CarPart::WheelFL, CarPart::WheelFR, CarPart::WheelRL, CarPart::WheelRR, CarPart::SteeringWheel};
    for (CarPart part : required)
        if (!hasPart(part)) *note += std::string("No object named ") + kPartNames[static_cast<int>(part)] + ", it will not move\n";

    const glm::vec3 size = all.hi - all.lo;
    if (size.z < 1.0f || size.z > 6.0f || size.x > size.z) {
        char line[128];
        std::snprintf(line, sizeof line, "Car is %.2f wide and %.2f long, expected metres with the nose towards -Y\n", size.x, size.z);
        *note += line;
    }
    if (hasPart(CarPart::WheelFL) && hasPart(CarPart::WheelRL) &&
        (pivot(CarPart::WheelFL).z <= pivot(CarPart::WheelRL).z || pivot(CarPart::WheelFL).x <= 0.0f))
        *note += "wheel_FL is not at the front left, the nose should point towards -Y with the car's left at +X\n";

    const unsigned char white[4] = {255, 255, 255, 255};
    glGenTextures(1, &white_);
    glBindTexture(GL_TEXTURE_2D, white_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}
