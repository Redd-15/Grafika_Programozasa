// View: the real car, loaded from a glTF binary exported from Blender (see README):
// GPU meshes grouped by moving part and material, plus metallic-roughness materials
// and textures. Which part moves how is decided by the model (model/car.h).
#pragma once
#include <deque>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "model/car.h"
#include "view/gl/mesh.h"

struct MaterialMap {
    unsigned texture = 0;                      // 0 = no map
    int uvSet = 0;                             // 0 or 1
    glm::mat3 uvTransform = glm::mat3(1.0f);   // Mapping node in Blender
};

enum class AlphaMode { Opaque, Mask, Blend };

// The defaults are what objects without a material get.
struct ModelMaterial {
    MaterialMap baseColor, metallicRoughness, normal, occlusion, emissive;
    glm::vec4 baseColorFactor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.7f;
    glm::vec3 emissiveFactor = glm::vec3(0.0f);
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
};

// Everything in one part that uses one material. Vertices are in car space.
struct ModelSurface {
    CarPart part = CarPart::Body;
    int material = 0;
    glm::vec3 center = glm::vec3(0.0f);   // sorts transparent surfaces
    Mesh mesh;
};

// Parts are found by Blender object name (or the name of a parent Empty); every other
// object belongs to the body.
class GltfCar {
public:
    GltfCar() = default;
    ~GltfCar();
    GltfCar(const GltfCar&) = delete;
    GltfCar& operator=(const GltfCar&) = delete;

    // `note` says why loading failed, or on success lists anything wrong with the model.
    bool load(const std::string& path, std::string* note);

    bool loaded() const { return !surfaces_.empty(); }
    bool hasPart(CarPart part) const { return hasPart_[static_cast<int>(part)]; }
    glm::vec3 pivot(CarPart part) const { return pivots_[static_cast<int>(part)]; }
    const std::deque<ModelSurface>& surfaces() const { return surfaces_; }
    const ModelMaterial& material(int index) const { return materials_[index]; }
    unsigned whiteTexture() const { return white_; }   // stands in for a missing map

private:
    std::deque<ModelSurface> surfaces_;
    std::vector<ModelMaterial> materials_;
    std::vector<unsigned> textures_;
    unsigned white_ = 0;
    glm::vec3 pivots_[kCarPartCount] = {};
    bool hasPart_[kCarPartCount] = {};
};
