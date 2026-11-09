#include "controller/application.h"

#include "view/gl/gl_loader.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>

#include "controller/scene_controller.h"
#include "controller/source_controller.h"
#include "model/car.h"
#include "model/sources/source_config.h"
#include "model/telemetry.h"
#include "view/scene/car_renderer.h"
#include "view/scene/orbit_camera.h"
#include "view/ui/dashboard_view.h"
#include "view/ui/source_window.h"
#include "view/ui/style.h"

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

// First start: begin from the car's default mapping (built from the DBC), but keep
// saving to data_source.json so user edits are never overwritten by the default.
SourceConfig LoadSourceConfig(const char* argv0)
{
    SourceConfig config;
    const std::filesystem::path dir = ExeDir(argv0);
    const std::string userFile = (dir / "data_source.json").string();
    if (!config.load(userFile) && config.load((dir / "assets" / "data_source.default.json").string()))
        config.path = userFile;
    return config;
}

} // namespace

Application::Application(int, char** argv) : argv_(argv) {}

Application::~Application() { shutdownWindow(); }

bool Application::initWindow()
{
    glfwSetErrorCallback(GlfwError);
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    int winW = 1600, winH = 900;
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
        float sx = 1.0f, sy = 1.0f;
        glfwGetMonitorContentScale(monitor, &sx, &sy);
        uiScale_ = std::max(1.0f, sx);
        if (const GLFWvidmode* mode = glfwGetVideoMode(monitor)) {
            winW = std::min(winW, static_cast<int>(mode->width * 0.92f / uiScale_));
            winH = std::min(winH, static_cast<int>(mode->height * 0.88f / uiScale_));
        }
    }

    window_ = glfwCreateWindow(winW, winH, "FS Telemetry", nullptr, nullptr);
    if (!window_) return false;
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    glfwSetWindowSizeLimits(window_, 1000, 640, GLFW_DONT_CARE, GLFW_DONT_CARE);

    const char* missing = nullptr;
    if (!LoadGLFunctions(&missing)) {
        std::fprintf(stderr, "Failed to load OpenGL function %s\n", missing ? missing : "?");
        return false;
    }
    glEnable(GL_MULTISAMPLE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    return true;
}

void Application::shutdownWindow()
{
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (window_) glfwDestroyWindow(window_);
    window_ = nullptr;
    glfwTerminate();
}

int Application::run()
{
    if (!initWindow()) return 1;
    const UiFonts fonts = LoadFonts(uiScale_);
    ApplyStyle(uiScale_);

    // GL objects (renderer) are destroyed at the end of this scope, while the context is alive.
    // --- Model -------------------------------------------------------------------
    SourceConfig sources = LoadSourceConfig(argv_[0]);
    Telemetry telemetry(CreateSource(sources));
    Car car;

    // --- View --------------------------------------------------------------------
    CarRenderer renderer;
    OrbitCamera camera;
    SourceWindow sourceWindow;
    bool sourceWindowOpen = false;
    std::string error;
    if (!renderer.init(FindModelDir(argv_[0]), car.geometry(), &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    // A real car model brings its own wheelbase and track.
    CarGeometry measured = car.geometry();
    if (renderer.measureGeometry(measured)) car.setGeometry(measured);

    // --- Controller --------------------------------------------------------------
    SceneController sceneController(camera);
    SourceController sourceController(sources, telemetry);

    ImGuiIO& io = ImGui::GetIO();
    double last = glfwGetTime();
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED)) {
            glfwWaitEventsTimeout(0.1);
            continue;
        }
        const double now = glfwGetTime();
        const float dt = static_cast<float>(std::min(now - last, 0.1));
        last = now;

        // 1) Model: new data in, car motion calculated.
        telemetry.update(now, dt);
        car.update(telemetry.frame(), dt);

        // 2) View (UI) + controller input.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        DashboardOptions options;
        options.fonts = &fonts;
        options.scale = uiScale_;
        options.time = now;
        options.placeholderModel = renderer.placeholderBody();
        options.modelNote = renderer.modelNote().c_str();
        options.sourcePillEnabled = !sourceWindowOpen;
        const DashboardResult ui = DrawDashboard(telemetry, options);
        if (ui.sourcePillClicked) sourceWindowOpen = true;
        sourceWindow.draw(&sourceWindowOpen, sourceController, fonts, uiScale_);
        sceneController.handleInput(ui.scene, dt);
        ImGui::Render();

        // 3) View (3D) under the UI.
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window_, &fbW, &fbH);
        const float sx = io.DisplaySize.x > 0 ? fbW / io.DisplaySize.x : 1.0f;
        const float sy = io.DisplaySize.y > 0 ? fbH / io.DisplaySize.y : 1.0f;
        const SceneViewport vp{static_cast<int>(ui.scene.min.x * sx), static_cast<int>(fbH - ui.scene.max.y * sy),
                               static_cast<int>((ui.scene.max.x - ui.scene.min.x) * sx),
                               static_cast<int>((ui.scene.max.y - ui.scene.min.y) * sy)};
        renderer.render(car, camera, fbW, fbH, vp, static_cast<float>(now));

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
    if (sourceWindowOpen) sources.save();
    return 0;
}
