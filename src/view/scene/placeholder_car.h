// View: procedural stand-in meshes for when no car model files are present.
#pragma once
#include <glm/glm.hpp>

#include "model/car.h"
#include "view/gl/mesh.h"

namespace placeholder {
inline const glm::vec3 kBodyWhite(0.86f, 0.88f, 0.91f);
inline const glm::vec3 kAccentBlue(0.08f, 0.40f, 0.95f);
inline const glm::vec3 kCarbon(0.07f, 0.075f, 0.085f);
inline const glm::vec3 kDarkMetal(0.18f, 0.19f, 0.21f);
inline const glm::vec3 kCockpit(0.015f, 0.018f, 0.022f);
constexpr float kWheelRadius = 0.225f;   // radius the wheel mesh is modelled at
} // namespace placeholder

// Formula Student car body (no wheels, no steering wheel), laid out by `g`.
void BuildPlaceholderBody(MeshBuilder& mb, const CarGeometry& g);
// Wheel centred at the origin, axle along X, outer (rim) face towards +X.
void BuildPlaceholderWheel(MeshBuilder& mb, float radius);
// Steering wheel in the XY plane, facing the driver (-Z), 12 o'clock = +Y.
void BuildPlaceholderSteeringWheel(MeshBuilder& mb);
