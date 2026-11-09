#include "controller/scene_controller.h"

#include <imgui.h>

void SceneController::handleInput(const SceneRect& r, float dt)
{
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mouse = io.MousePos;
    const bool overScene = mouse.x >= r.min.x && mouse.x <= r.max.x && mouse.y >= r.min.y && mouse.y <= r.max.y;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && overScene && !io.WantCaptureMouse) dragging_ = true;
    if (dragging_) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            camera_.drag(io.MouseDelta.x, dt);
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        } else {
            dragging_ = false;
            camera_.release();
        }
    } else if (overScene && !io.WantCaptureMouse) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    camera_.update(dt);
}
