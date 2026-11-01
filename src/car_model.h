// The real car, loaded from a glTF binary exported from Blender (see README):
// meshes grouped by moving part and material, plus metallic-roughness materials.
#pragma once
#include <deque>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "mesh.h"

// The parts that move, found by Blender object name (or the name of a parent Empty).
// Every other object belongs to the static body.
enum class CarPart { Body, WheelFL, WheelFR, WheelRL, WheelRR, UprightFL, UprightFR, SteeringWheel, Count };
constexpr int kCarPartCount = static_cast<int>(CarPart::Count);

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

class CarModel {
public:
    CarModel() = default;
    ~CarModel();
    CarModel(const CarModel&) = delete;
    CarModel& operator=(const CarModel&) = delete;

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
