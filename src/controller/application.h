// Controller: owns the window and the main loop and connects the parts:
//   model      Telemetry (data source, frame, history), Car (kinematics)
//   view       CarRenderer + OrbitCamera (3D), dashboard + data-source window (ImGui)
//   controller SceneController (mouse -> camera), SourceController (data-source actions)
#pragma once

struct GLFWwindow;

class Application {
public:
    Application(int argc, char** argv);
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    bool initWindow();
    void shutdownWindow();

    char** argv_;
    GLFWwindow* window_ = nullptr;
    float uiScale_ = 1.0f;
};
