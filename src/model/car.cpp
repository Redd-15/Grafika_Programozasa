#include "model/car.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace {
constexpr float kPi = 3.14159265358979f;
}

void Car::update(const TelemetryFrame& f, float dt)
{
    pose_.wheelRadius = std::max(0.05f, f.settings.wheelSizeMm / 2000.0f);
    const float speed = f.speedKmh / 3.6f;   // m/s
    pose_.wheelSpin = std::fmod(pose_.wheelSpin + speed / pose_.wheelRadius * dt, 2.0f * kPi);
    pose_.distance = std::fmod(pose_.distance + speed * dt, 1000.0f);

    // Positive steering = turning right; the car's right side is -X.
    pose_.steeringWheelAngle = glm::radians(f.steeringDeg);
    pose_.roadWheelAngle = -glm::radians(f.steeringDeg / geometry_.steeringRatio);
}

glm::mat4 Car::wheelTransform(int i, float meshWheelRadius) const
{
    const CarGeometry& g = geometry_;
    const bool front = i < 2;
    const bool left = (i % 2) == 0;
    const float track = front ? g.frontTrack : g.rearTrack;
    const float z = front ? g.frontAxleZ : g.frontAxleZ - g.wheelbase;
    glm::mat4 m = glm::translate(glm::mat4(1.0f), {left ? track * 0.5f : -track * 0.5f, pose_.wheelRadius, z});
    if (front) m = glm::rotate(m, pose_.roadWheelAngle, glm::vec3(0, 1, 0));
    m = glm::rotate(m, pose_.wheelSpin, glm::vec3(1, 0, 0));
    if (!left) m = glm::rotate(m, kPi, glm::vec3(0, 1, 0));   // mirror so the rim faces outwards
    return glm::scale(m, glm::vec3(pose_.wheelRadius / meshWheelRadius));   // follow the wheel-size setting
}

glm::mat4 Car::steeringWheelTransform() const
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), geometry_.steeringWheelPos);
    m = glm::rotate(m, glm::radians(geometry_.steeringWheelTiltDeg), glm::vec3(1, 0, 0));
    return glm::rotate(m, pose_.steeringWheelAngle, glm::vec3(0, 0, 1));
}

glm::mat4 Car::partRotation(CarPart part) const
{
    const glm::mat4 steer = glm::rotate(glm::mat4(1.0f), pose_.roadWheelAngle, glm::vec3(0, 1, 0));
    const glm::mat4 spin = glm::rotate(glm::mat4(1.0f), pose_.wheelSpin, glm::vec3(1, 0, 0));
    switch (part) {
    case CarPart::WheelFL:
    case CarPart::WheelFR:   return steer * spin;
    case CarPart::WheelRL:
    case CarPart::WheelRR:   return spin;
    case CarPart::UprightFL:
    case CarPart::UprightFR: return steer;
    case CarPart::SteeringWheel: {
        // Around the column, which is tilted back like the mount.
        const float tilt = glm::radians(geometry_.steeringWheelTiltDeg);
        return glm::rotate(glm::mat4(1.0f), pose_.steeringWheelAngle, glm::vec3(0.0f, -std::sin(tilt), std::cos(tilt)));
    }
    default: return glm::mat4(1.0f);
    }
}
