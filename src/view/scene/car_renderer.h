// View: draws the 3D scene: background, reflective floor and the car. How the car's
// parts move comes from the model (Car); this class only turns it into pixels.
// The whole car comes from assets/models/car.glb if present. Otherwise each part is
// loaded from assets/models/*.obj, or a procedural placeholder is used.
#pragma once
#include <string>

#include <glm/glm.hpp>

#include "model/car.h"
#include "view/gl/mesh.h"
#include "view/scene/gltf_car.h"
#include "view/scene/orbit_camera.h"

struct SceneViewport { int x, y, w, h; };   // framebuffer pixels, origin bottom-left

class CarRenderer {
public:
    CarRenderer();
    ~CarRenderer();
    CarRenderer(const CarRenderer&) = delete;
    CarRenderer& operator=(const CarRenderer&) = delete;

    // `geometry` lays out the placeholder car if no model file is found.
    bool init(const std::string& modelDir, const CarGeometry& geometry, std::string* error);
    void render(const Car& car, const OrbitCamera& camera, int fbWidth, int fbHeight, SceneViewport viewport,
                float time);

    // When car.glb has all four wheels, copies its real wheelbase and track into `geometry`.
    bool measureGeometry(CarGeometry& geometry) const;

    bool placeholderBody() const { return placeholderBody_; }
    const std::string& modelNote() const { return modelNote_; }   // problems with car.glb, shown in the 3D view

private:
    struct PbrUniforms {
        int model, baseColor, metallic, roughness, emissive, normalScale, occlusion, alphaCutoff, hasNormalMap, uvSet, uvXform;
    };

    bool loadGltf(const std::string& path);
    void drawCar(const Car& car, unsigned program, const glm::vec3& eye, bool reflection);
    void drawGltf(const Car& car, const glm::vec3& eye);
    void bindMaterial(const ModelMaterial& material);
    glm::mat4 partTransform(const Car& car, CarPart part) const;

    GltfCar gltf_;
    std::string modelNote_;
    Mesh body_, wheel_, steeringWheel_, floor_;
    unsigned meshProgram_ = 0, pbrProgram_ = 0, floorProgram_ = 0, bgProgram_ = 0, emptyVao_ = 0;
    PbrUniforms pbr_{};
    bool placeholderBody_ = true;
};
