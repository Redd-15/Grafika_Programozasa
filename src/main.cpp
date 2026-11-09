// Formula Student telemetry dashboard: 3D car in the middle, live data on the sides.
//
// The code follows Model-View-Controller:
//   src/model/       data and calculations (telemetry, data sources, car kinematics, alarms)
//   src/view/        rendering only (OpenGL scene, ImGui dashboard and windows)
//   src/controller/  window + main loop, input handling, data-source actions
#include "controller/application.h"

int main(int argc, char** argv)
{
    Application app(argc, argv);
    return app.run();
}
