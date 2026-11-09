cmake_minimum_required(VERSION 3.20)
project(FSTelemetry LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Some dependencies still declare very old minimum CMake versions.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw  GIT_REPOSITORY https://github.com/glfw/glfw.git   GIT_TAG 3.4       GIT_SHALLOW TRUE)
FetchContent_Declare(glm   GIT_REPOSITORY https://github.com/g-truc/glm.git  GIT_TAG 1.0.1     GIT_SHALLOW TRUE)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.9b GIT_SHALLOW TRUE)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
                     DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(cgltf GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git GIT_TAG v1.15 GIT_SHALLOW TRUE)
FetchContent_Declare(stb URL https://github.com/nothings/stb/archive/003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5.tar.gz
                     DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(glfw glm imgui json cgltf stb)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR}/misc/cpp)
target_link_libraries(imgui PUBLIC glfw)

# Model: data and calculations (no OpenGL / ImGui). A library so tools can reuse it.
add_library(fs_model STATIC
  src/model/alarms.cpp
  src/model/car.cpp
  src/model/car_state.cpp
  src/model/telemetry.cpp
  src/model/telemetry_history.cpp
  src/model/sources/json_flatten.cpp
  src/model/sources/signal_mapping.cpp
  src/model/sources/simulated_source.cpp
  src/model/sources/source_config.cpp
  src/model/sources/udp_source.cpp)
target_include_directories(fs_model PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(fs_model PUBLIC glm::glm nlohmann_json::nlohmann_json)
target_compile_definitions(fs_model PUBLIC GLM_ENABLE_EXPERIMENTAL)
if(WIN32)
  target_link_libraries(fs_model PUBLIC ws2_32)
endif()

add_executable(fs_telemetry
  src/main.cpp
  # View: rendering only
  src/view/gl/gl_loader.cpp
  src/view/gl/mesh.cpp
  src/view/gl/shader.cpp
  src/view/scene/car_renderer.cpp
  src/view/scene/gltf_car.cpp
  src/view/scene/orbit_camera.cpp
  src/view/scene/placeholder_car.cpp
  src/view/ui/dashboard_view.cpp
  src/view/ui/source_window.cpp
  src/view/ui/style.cpp
  src/view/ui/widgets.cpp
  # Controller: main loop, input, actions
  src/controller/application.cpp
  src/controller/scene_controller.cpp
  src/controller/source_controller.cpp)
target_link_libraries(fs_telemetry PRIVATE fs_model imgui)
target_include_directories(fs_telemetry PRIVATE ${cgltf_SOURCE_DIR} ${stb_SOURCE_DIR})
target_compile_definitions(fs_telemetry PRIVATE GLFW_INCLUDE_NONE)

# Test tool: sends simulated CAN messages (DBC names, as Node-RED does) over UDP.
add_executable(can_sim tools/can_sim/main.cpp)
target_link_libraries(can_sim PRIVATE fs_model)

if(MINGW)
  # Self-contained exes that run outside the MSYS2 shell.
  target_link_options(fs_telemetry PRIVATE -static -static-libgcc -static-libstdc++)
  target_link_options(can_sim PRIVATE -static -static-libgcc -static-libstdc++)
endif()
if(MINGW)
  # No console window in Release builds (MinGW's CRT still calls main()).
  set_target_properties(fs_telemetry PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
endif()

# Models placed in assets/models are picked up at runtime (see README).
add_custom_command(TARGET fs_telemetry POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:fs_telemetry>/assets)
