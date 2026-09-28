// 3D view of the car: body, four wheels and the steering wheel are separate
// parts so the wheels can steer/spin and the steering wheel can turn from
// telemetry. Each part is loaded from assets/models/*.obj if present,
// otherwise a procedural placeholder is used.
#pragma once
#include <string>

#include <glm/glm.hpp>

#include "mesh.h"

struct TelemetryFrame;

// Where the parts sit on the car (metres; x = left, y = up, z = forward).
struct CarGeometry {
    float wheelbase = 1.55f;
    float frontTrack = 1.20f;
    float rearTrack = 1.16f;
    float frontAxleZ = 0.80f;
    float steeringRatio = 5.0f;                          // steering wheel deg : road wheel deg
    glm::vec3 steeringWheelPos = {0.0f, 0.64f, 0.30f};
    float steeringWheelTiltDeg = 22.0f;
};

struct SceneViewport { int x, y, w, h; };   // framebuffer pixels, origin bottom-left

class CarScene {
public:
    CarScene();
    ~CarScene();

    bool init(const std::string& modelDir, std::string* error);
    void update(const TelemetryFrame& frame, float dt);
    void render(int fbWidth, int fbHeight, SceneViewport viewport, float time);

    // Horizontal-only orbit driven by mouse drags (pixels).
    void drag(float dxPixels, float dt);
    void release() { dragging_ = false; }

    bool placeholderBody() const { return placeholderBody_; }
    const CarGeometry& geometry() const { return geo_; }

private:
    void drawCar(unsigned program, bool reflection);
    glm::mat4 wheelTransform(int index) const;
    glm::mat4 steeringWheelTransform() const;

    CarGeometry geo_;
    Mesh body_, wheel_, steeringWheel_, floor_;
    unsigned meshProgram_ = 0, floorProgram_ = 0, bgProgram_ = 0, emptyVao_ = 0;
    bool placeholderBody_ = true;

    float wheelRadius_ = 0.225f;
    float wheelSpin_ = 0.0f;        // rad
    float roadWheelAngle_ = 0.0f;   // rad, + = left
    float steeringWheelAngle_ = 0.0f;

    float yaw_ = 0.75f, yawVelocity_ = 0.0f;
    float floorScroll_ = 0.0f;  // m, makes the grid move with vehicle speed
    bool dragging_ = false;
};
