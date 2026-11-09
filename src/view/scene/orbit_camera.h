// View: camera that orbits the car horizontally only, with a little inertia after a drag.
#pragma once
#include <glm/glm.hpp>

struct CameraMatrices {
    glm::vec3 eye;
    glm::mat4 viewProj;
};

class OrbitCamera {
public:
    // Turns the camera by a mouse movement in pixels.
    void drag(float dxPixels, float dt);
    void release() { dragging_ = false; }
    // Lets the camera coast and slow down after a drag.
    void update(float dt);

    // Pulls back when the viewport is narrow so the whole car stays in view.
    CameraMatrices matrices(int viewportWidth, int viewportHeight) const;

private:
    float yaw_ = 0.75f;          // rad around the vertical axis
    float yawVelocity_ = 0.0f;   // rad/s
    bool dragging_ = false;
};
