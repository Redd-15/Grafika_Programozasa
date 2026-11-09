// Model: the car's physical data and the calculations that turn telemetry into
// motion of its parts (wheel spin, steering, steering wheel). No rendering here:
// the view only asks for the resulting transforms.
#pragma once
#include <glm/glm.hpp>

#include "model/telemetry_frame.h"

// The parts that move independently. Every other piece of the car belongs to the body.
enum class CarPart { Body, WheelFL, WheelFR, WheelRL, WheelRR, UprightFL, UprightFR, SteeringWheel, Count };
constexpr int kCarPartCount = static_cast<int>(CarPart::Count);

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

// The car's current motion, derived from telemetry each frame.
struct CarPose {
    float wheelRadius = 0.225f;        // m, from the wheel-size setting
    float wheelSpin = 0.0f;            // rad, wraps at 2 pi
    float roadWheelAngle = 0.0f;       // rad, + = left (the car's right side is -X)
    float steeringWheelAngle = 0.0f;   // rad, + = clockwise as seen by the driver
    float distance = 0.0f;             // m travelled, wraps at 1000 m
};

class Car {
public:
    const CarGeometry& geometry() const { return geometry_; }
    void setGeometry(const CarGeometry& g) { geometry_ = g; }
    const CarPose& pose() const { return pose_; }

    // Integrates wheel rotation and distance, and follows the steering input.
    void update(const TelemetryFrame& frame, float dt);

    // Places a wheel mesh modelled around the origin (axle along X, rim towards +X, radius
    // `meshWheelRadius`) at wheel `index` (0 FL, 1 FR, 2 RL, 3 RR), steered and spinning.
    glm::mat4 wheelTransform(int index, float meshWheelRadius) const;
    // Places a steering-wheel mesh modelled in the XY plane at the mount, turned by the input.
    glm::mat4 steeringWheelTransform() const;
    // Rotation of a part that is already in place, around its own pivot (applied by the caller).
    glm::mat4 partRotation(CarPart part) const;

private:
    CarGeometry geometry_;
    CarPose pose_;
};
