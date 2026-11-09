#include "view/scene/orbit_camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

void OrbitCamera::drag(float dxPixels, float dt)
{
    const float delta = -dxPixels * 0.008f;
    yaw_ += delta;
    dragging_ = true;
    if (dt > 0.0f) yawVelocity_ = glm::mix(yawVelocity_, delta / dt, 0.5f);
}

void OrbitCamera::update(float dt)
{
    if (dragging_) return;
    yaw_ += yawVelocity_ * dt;
    yawVelocity_ *= std::exp(-3.5f * dt);
}

CameraMatrices OrbitCamera::matrices(int w, int h) const
{
    const float aspect = static_cast<float>(w) / std::max(h, 1);
    const float fovY = glm::radians(30.0f);
    const float halfFovX = std::atan(std::tan(fovY * 0.5f) * aspect);
    const float dist = std::max(4.9f, 1.7f / std::tan(halfFovX));
    const float pitch = glm::radians(17.0f);
    const glm::vec3 target(0.0f, 0.42f, -0.05f);
    CameraMatrices m;
    m.eye = target + dist * glm::vec3(std::cos(pitch) * std::sin(yaw_), std::sin(pitch), std::cos(pitch) * std::cos(yaw_));
    m.viewProj = glm::perspective(fovY, aspect, 0.1f, 60.0f) * glm::lookAt(m.eye, target, glm::vec3(0, 1, 0));
    return m;
}
