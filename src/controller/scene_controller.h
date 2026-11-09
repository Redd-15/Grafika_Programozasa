// Controller: turns mouse input over the 3D area into camera movement
// (horizontal orbit by dragging with the left button).
#pragma once

#include "view/scene/orbit_camera.h"
#include "view/ui/dashboard_view.h"

class SceneController {
public:
    explicit SceneController(OrbitCamera& camera) : camera_(camera) {}
    // Call once per frame after the UI is built (needs ImGui's mouse state).
    void handleInput(const SceneRect& sceneArea, float dt);

private:
    OrbitCamera& camera_;
    bool dragging_ = false;
};
