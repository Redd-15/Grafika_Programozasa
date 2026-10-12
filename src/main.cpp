// Formula Student telemetry dashboard: 3D car in the middle, live data on the sides.
#include "gl_loader.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>

#include "car_scene.h"
#include "data_source.h"
#include "telemetry.h"
#include "ui.h"

namespace {

void GlfwError(int code, const char* msg) { std::fprintf(stderr, "GLFW error %d: %s\n", code, msg); }

std::filesystem::path ExeDir(const char* argv0)
{
    std::error_code ec;
    return std::filesystem::absolute(std::filesystem::path(argv0), ec).parent_path();
}

// Models live next to the executable (copied by the build) or in the working directory.
std::string FindModelDir(const char* argv0)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path exeDir = ExeDir(argv0);
    for (const fs::path& p : {exeDir / "assets" / "models", fs::path("assets") / "models"})
        if (fs::is_directory(p, ec)) return p.string();
    return "assets/models";
}

} // namespace

int main(int argc, char** argv)
{
    (void)argc;
    glfwSetErrorCallback(GlfwError);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    float scale = 1.0f;
    int winW = 1600, winH = 900;
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
        float sx = 1.0f, sy = 1.0f;
        glfwGetMonitorContentScale(monitor, &sx, &sy);
        scale = std::max(1.0f, sx);
        if (const GLFWvidmode* mode = glfwGetVideoMode(monitor)) {
            winW = std::min(winW, static_cast<int>(mode->width * 0.92f / scale));
            winH = std::min(winH, static_cast<int>(mode->height * 0.88f / scale));
        }
    }

    GLFWwindow* window = glfwCreateWindow(winW, winH, "FS Telemetry", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetWindowSizeLimits(window, 1000, 640, GLFW_DONT_CARE, GLFW_DONT_CARE);

    const char* missing = nullptr;
    if (!LoadGLFunctions(&missing)) {
        std::fprintf(stderr, "Failed to load OpenGL function %s\n", missing ? missing : "?");
        return 1;
    }
    glEnable(GL_MULTISAMPLE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    const UiFonts fonts = LoadFonts(scale);
    ApplyStyle(scale);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    int exitCode = 0;
    {   // GL objects must be destroyed while the context is still alive.
    CarScene scene;
    std::string error;
    if (!scene.init(FindModelDir(argv[0]), &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        exitCode = 1;
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    // Data-source choice and CAN signal mapping, edited from the top-bar source pill.
    SourceConfig sources;
    {
        // First start: begin from the car's default mapping (built from the DBC), but keep
        // saving to data_source.json so user edits are never overwritten by the default.
        const std::filesystem::path dir = ExeDir(argv[0]);
        const std::string userFile = (dir / "data_source.json").string();
        if (!sources.load(userFile) && sources.load((dir / "assets" / "data_source.default.json").string()))
            sources.path = userFile;
    }
    Telemetry telemetry(CreateSource(sources));

    double last = glfwGetTime();
    bool draggingScene = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
            glfwWaitEventsTimeout(0.1);
            continue;
        }
        const double now = glfwGetTime();
        const float dt = static_cast<float>(std::min(now - last, 0.1));
        last = now;

        telemetry.update(now, dt);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const SceneRect sceneRect = DrawDashboard(telemetry, sources, fonts, scale, now, scene.placeholderBody());

        // Horizontal orbit: drag with the left mouse button inside the 3D area.
        const ImVec2 mouse = io.MousePos;
        const bool overScene = mouse.x >= sceneRect.min.x && mouse.x <= sceneRect.max.x &&
                               mouse.y >= sceneRect.min.y && mouse.y <= sceneRect.max.y;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && overScene && !io.WantCaptureMouse) draggingScene = true;
        if (draggingScene) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                scene.drag(io.MouseDelta.x, dt);
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            } else {
                draggingScene = false;
                scene.release();
            }
        } else if (overScene && !io.WantCaptureMouse) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        scene.update(telemetry.frame(), dt);
        ImGui::Render();

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        const float sx = io.DisplaySize.x > 0 ? fbW / io.DisplaySize.x : 1.0f;
        const float sy = io.DisplaySize.y > 0 ? fbH / io.DisplaySize.y : 1.0f;
        SceneViewport vp{static_cast<int>(sceneRect.min.x * sx), static_cast<int>(fbH - sceneRect.max.y * sy),
                         static_cast<int>((sceneRect.max.x - sceneRect.min.x) * sx),
                         static_cast<int>((sceneRect.max.y - sceneRect.min.y) * sy)};
        scene.render(fbW, fbH, vp, static_cast<float>(now));

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}
