#include "view/scene/placeholder_car.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace {

constexpr float kPi = 3.14159265358979f;

glm::mat4 BoxXf(glm::vec3 center, glm::vec3 size, float rotXDeg = 0.0f)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), center);
    m = glm::rotate(m, glm::radians(rotXDeg), glm::vec3(1, 0, 0));
    return glm::scale(m, size);
}

} // namespace

using namespace placeholder;

void BuildPlaceholderBody(MeshBuilder& mb, const CarGeometry& g)
{
    using S = MeshBuilder::Section;
    // Monocoque, nose to rear.
    mb.addLoft({S{1.58f, 0, 0.24f, 0.14f, 0.10f},
                S{1.05f, 0, 0.30f, 0.36f, 0.30f},
                S{0.45f, 0, 0.36f, 0.52f, 0.42f},
                S{-0.45f, 0, 0.36f, 0.56f, 0.42f},
                S{-0.60f, 0, 0.39f, 0.56f, 0.48f},
                S{-1.00f, 0, 0.36f, 0.44f, 0.38f},
                S{-1.22f, 0, 0.33f, 0.28f, 0.26f}},
               kBodyWhite, kCarbon);
    // Blue nose tip.
    mb.addLoft({S{1.66f, 0, 0.225f, 0.08f, 0.06f}, S{1.58f, 0, 0.24f, 0.145f, 0.105f}}, kAccentBlue, kAccentBlue);

    // Cockpit opening and rims.
    mb.addBox({0, 0.572f, -0.02f}, {0.40f, 0.012f, 0.80f}, kCockpit);
    mb.addBeam({0.21f, 0.575f, 0.40f}, {0.21f, 0.575f, -0.42f}, 0.03f, kAccentBlue);
    mb.addBeam({-0.21f, 0.575f, 0.40f}, {-0.21f, 0.575f, -0.42f}, 0.03f, kAccentBlue);

    // Sidepods.
    for (float sx : {1.0f, -1.0f}) {
        mb.addLoft({S{0.38f, sx * 0.43f, 0.28f, 0.16f, 0.20f},
                    S{0.25f, sx * 0.43f, 0.30f, 0.26f, 0.30f},
                    S{-0.50f, sx * 0.43f, 0.30f, 0.26f, 0.30f},
                    S{-0.82f, sx * 0.40f, 0.25f, 0.16f, 0.16f}},
                   kAccentBlue, kCockpit);
        mb.addBox({sx * 0.43f, 0.455f, -0.12f}, {0.265f, 0.01f, 0.6f}, kBodyWhite);   // top stripe
    }

    // Undertray and diffuser.
    mb.addBox({0, 0.07f, -0.1f}, {1.02f, 0.02f, 1.9f}, kCarbon);
    mb.addBox(BoxXf({0, 0.12f, -1.2f}, {0.9f, 0.015f, 0.45f}, 14.0f), kCarbon);

    // Main hoop, front hoop and bracing.
    const float hoopZ = -0.44f;
    mb.addBeam({0.26f, 0.58f, hoopZ}, {0.19f, 1.02f, hoopZ - 0.03f}, 0.035f, kDarkMetal);
    mb.addBeam({-0.26f, 0.58f, hoopZ}, {-0.19f, 1.02f, hoopZ - 0.03f}, 0.035f, kDarkMetal);
    mb.addBeam({0.19f, 1.02f, hoopZ - 0.03f}, {-0.19f, 1.02f, hoopZ - 0.03f}, 0.035f, kDarkMetal);
    mb.addBeam({0.19f, 1.0f, hoopZ - 0.03f}, {0.12f, 0.6f, -0.98f}, 0.028f, kDarkMetal);
    mb.addBeam({-0.19f, 1.0f, hoopZ - 0.03f}, {-0.12f, 0.6f, -0.98f}, 0.028f, kDarkMetal);
    mb.addBeam({0.24f, 0.57f, 0.46f}, {0.14f, 0.70f, 0.46f}, 0.03f, kDarkMetal);
    mb.addBeam({-0.24f, 0.57f, 0.46f}, {-0.14f, 0.70f, 0.46f}, 0.03f, kDarkMetal);
    mb.addBeam({0.14f, 0.70f, 0.46f}, {-0.14f, 0.70f, 0.46f}, 0.03f, kDarkMetal);
    mb.addBox({0, 0.74f, -0.48f}, {0.26f, 0.2f, 0.08f}, kCockpit);                   // headrest

    // Driver (helmet with visor + shoulders).
    mb.addBox({0, 0.62f, -0.24f}, {0.40f, 0.20f, 0.24f}, glm::vec3(0.10f, 0.22f, 0.55f));
    mb.addSphere({0, 0.84f, -0.22f}, 0.135f, 28, glm::vec3(0.92f, 0.93f, 0.95f));
    MeshBuilder visor;   // flattened ellipsoid poking out of the helmet front = visor band
    visor.addSphere({0, 0, 0}, 1.0f, 28, glm::vec3(0.03f, 0.04f, 0.06f));
    mb.append(visor, glm::scale(glm::translate(glm::mat4(1.0f), {0, 0.85f, -0.20f}), {0.13f, 0.055f, 0.13f}));

    // Steering column (does not rotate with the wheel).
    glm::mat4 sw = glm::translate(glm::mat4(1.0f), g.steeringWheelPos) *
                   glm::rotate(glm::mat4(1.0f), glm::radians(g.steeringWheelTiltDeg), glm::vec3(1, 0, 0));
    mb.addBeam(glm::vec3(sw * glm::vec4(0, 0, 0.03f, 1)), glm::vec3(sw * glm::vec4(0, 0, 0.40f, 1)), 0.03f, kDarkMetal);

    // Motor / gearbox at the rear.
    mb.addBox({0, 0.30f, -1.12f}, {0.34f, 0.24f, 0.30f}, kDarkMetal);

    // Front wing.
    mb.addBox({0, 0.075f, 1.64f}, {1.36f, 0.02f, 0.32f}, kCarbon);
    mb.addBox(BoxXf({0, 0.13f, 1.52f}, {1.26f, 0.015f, 0.18f}, 22.0f), kAccentBlue);
    mb.addBox(BoxXf({0, 0.19f, 1.43f}, {1.20f, 0.012f, 0.13f}, 38.0f), kCarbon);
    for (float sx : {1.0f, -1.0f}) {
        mb.addBox({sx * 0.685f, 0.14f, 1.58f}, {0.014f, 0.19f, 0.46f}, kBodyWhite);
        mb.addBeam({sx * 0.04f, 0.23f, 1.45f}, {sx * 0.10f, 0.08f, 1.56f}, 0.02f, kCarbon);
    }

    // Rear wing.
    mb.addBox(BoxXf({0, 0.94f, -1.47f}, {1.0f, 0.02f, 0.36f}, 12.0f), kCarbon);
    mb.addBox(BoxXf({0, 1.04f, -1.31f}, {1.0f, 0.015f, 0.20f}, 35.0f), kAccentBlue);
    mb.addBox(BoxXf({0, 1.13f, -1.22f}, {1.0f, 0.012f, 0.15f}, 55.0f), kCarbon);
    for (float sx : {1.0f, -1.0f}) {
        mb.addBox({sx * 0.51f, 0.97f, -1.38f}, {0.014f, 0.44f, 0.58f}, kCarbon);
        mb.addBox({sx * 0.512f, 1.18f, -1.38f}, {0.016f, 0.025f, 0.58f}, kAccentBlue);
        mb.addBeam({sx * 0.12f, 0.42f, -1.08f}, {sx * 0.12f, 0.92f, -1.42f}, 0.025f, kCarbon);
    }

    // Suspension (upper/lower wishbones + pushrods) to each hub.
    const float rearZ = g.frontAxleZ - g.wheelbase;
    struct Corner { float hubX, hubZ, zFront, zRear, chassisX; };
    for (float sx : {1.0f, -1.0f}) {
        const Corner corners[2] = {
            {sx * (g.frontTrack * 0.5f - 0.1f), g.frontAxleZ, g.frontAxleZ + 0.2f, g.frontAxleZ - 0.22f, sx * 0.17f},
            {sx * (g.rearTrack * 0.5f - 0.1f), rearZ, rearZ + 0.25f, rearZ - 0.2f, sx * 0.18f},
        };
        for (const Corner& c : corners) {
            glm::vec3 upper(c.hubX, 0.31f, c.hubZ), lower(c.hubX, 0.15f, c.hubZ);
            mb.addBeam(upper, {c.chassisX, 0.40f, c.zFront}, 0.018f, kDarkMetal);
            mb.addBeam(upper, {c.chassisX, 0.40f, c.zRear}, 0.018f, kDarkMetal);
            mb.addBeam(lower, {c.chassisX, 0.17f, c.zFront}, 0.018f, kDarkMetal);
            mb.addBeam(lower, {c.chassisX, 0.17f, c.zRear}, 0.018f, kDarkMetal);
            mb.addBeam(lower + glm::vec3(-0.03f * (c.hubX > 0 ? 1 : -1), 0.01f, 0),
                       {c.chassisX * 1.1f, 0.50f, c.hubZ}, 0.016f, kAccentBlue);
            mb.addBox({c.hubX - (c.hubX > 0 ? 0.02f : -0.02f), 0.225f, c.hubZ}, {0.05f, 0.2f, 0.08f}, kDarkMetal);
        }
    }
}

void BuildPlaceholderWheel(MeshBuilder& mb, float radius)
{
    const float halfW = 0.10f;
    const float rimR = radius * 0.58f;
    const glm::vec3 rubber(0.055f, 0.057f, 0.062f), rubber2(0.085f, 0.088f, 0.095f);
    const glm::vec3 alu(0.72f, 0.74f, 0.78f), aluDark(0.25f, 0.26f, 0.28f);
    const glm::vec3 letter(0.95f, 0.78f, 0.10f);

    mb.addCylinderX(-halfW, halfW, radius, 60, rubber, rubber2, 2);                     // tread
    mb.addCylinderX(-halfW - 0.005f, halfW + 0.005f, radius - 0.018f, 60, rubber, rubber, 0);
    for (int side : {1, -1}) {
        float x = side * (halfW + 0.005f);
        mb.addAnnulusX(x, rimR, radius - 0.018f, 60, side > 0, rubber, rubber, 0);        // sidewall
        mb.addAnnulusX(x + side * 0.001f, radius * 0.80f, radius * 0.86f, 60, side > 0, letter, rubber, 5);
    }
    mb.addCylinderX(-halfW + 0.01f, halfW - 0.005f, rimR, 48, aluDark, aluDark, 0);     // rim barrel
    mb.addAnnulusX(halfW - 0.004f, rimR - 0.012f, rimR, 48, true, alu, alu, 0);          // rim lip
    mb.addAnnulusX(0.02f, 0.0f, rimR, 48, true, glm::vec3(0.03f), glm::vec3(0.03f), 0);  // dark inner
    mb.addAnnulusX(0.03f, rimR * 0.45f, rimR * 0.9f, 36, true, glm::vec3(0.45f), glm::vec3(0.30f), 3); // brake disc

    // Spokes (five-spoke centre-lock wheel) so rotation is clearly visible.
    for (int i = 0; i < 5; ++i) {
        float a = 2.0f * kPi * i / 5.0f;
        glm::vec3 dir(0, std::cos(a), std::sin(a));
        mb.addBeam(glm::vec3(0.075f, 0, 0) + dir * 0.03f, glm::vec3(0.07f, 0, 0) + dir * (rimR - 0.008f), 0.022f, alu);
    }
    mb.addCylinderX(0.05f, 0.09f, 0.035f, 24, alu, alu, 0);
    mb.addAnnulusX(0.09f, 0.0f, 0.035f, 24, true, glm::vec3(0.85f, 0.12f, 0.10f), glm::vec3(0.85f, 0.12f, 0.10f), 0);
}

void BuildPlaceholderSteeringWheel(MeshBuilder& mb)
{
    MeshBuilder ring;
    ring.addTorusZ({0, 0, 0}, 0.125f, 0.017f, 48, 12, glm::vec3(0.06f, 0.06f, 0.07f));
    mb.append(ring, glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 0.78f, 1.0f)));

    mb.addBox({0, 0.0f, 0.0f}, {0.20f, 0.095f, 0.035f}, kCarbon);
    mb.addBox({0, 0.008f, -0.019f}, {0.085f, 0.045f, 0.004f}, glm::vec3(0.15f, 0.55f, 1.0f));  // display
    mb.addBeam({0.09f, 0, 0}, {0.125f, 0, 0}, 0.025f, kCarbon);
    mb.addBeam({-0.09f, 0, 0}, {-0.125f, 0, 0}, 0.025f, kCarbon);
    mb.addBox({0, 0.125f * 0.78f, 0}, {0.03f, 0.03f, 0.04f}, glm::vec3(1.0f, 0.45f, 0.05f)); // top marker
    for (float sx : {1.0f, -1.0f})                                                           // buttons
        mb.addBox({sx * 0.07f, -0.02f, -0.02f}, {0.018f, 0.018f, 0.008f}, glm::vec3(0.9f, 0.2f, 0.15f));
}
