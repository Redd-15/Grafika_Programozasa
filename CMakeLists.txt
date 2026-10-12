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
FetchContent_MakeAvailable(glfw glm imgui json)

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

add_executable(fs_telemetry
  src/main.cpp
  src/gl_loader.cpp
  src/telemetry.cpp
  src/mesh.cpp
  src/car_scene.cpp
  src/ui.cpp
  src/data_source.cpp
  src/udp_source.cpp
  src/source_panel.cpp)
target_link_libraries(fs_telemetry PRIVATE imgui glm::glm nlohmann_json::nlohmann_json)
if(WIN32)
  target_link_libraries(fs_telemetry PRIVATE ws2_32)
endif()
target_compile_definitions(fs_telemetry PRIVATE GLFW_INCLUDE_NONE GLM_ENABLE_EXPERIMENTAL)

if(MINGW)
  # Self-contained exe that runs outside the MSYS2 shell.
  target_link_options(fs_telemetry PRIVATE -static -static-libgcc -static-libstdc++)
endif()
if(MINGW)
  # No console window in Release builds (MinGW's CRT still calls main()).
  set_target_properties(fs_telemetry PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
endif()

# Models placed in assets/models are picked up at runtime (see README).
add_custom_command(TARGET fs_telemetry POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:fs_telemetry>/assets)
