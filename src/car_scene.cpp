#include "car_scene.h"

#include "gl_loader.h"
#include "telemetry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kModelWheelRadius = 0.225f;   // radius the wheel mesh is modelled at

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------
const char* kMeshVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
uniform mat4 uModel;
uniform mat4 uViewProj;
uniform float uMirror;
out vec3 vWorld;
out vec3 vNormal;
out vec3 vColor;
void main() {
    vec4 w = uModel * vec4(aPos, 1.0);
    w.y *= uMirror;
    vec3 n = mat3(uModel) * aNormal;
    n.y *= uMirror;
    vWorld = w.xyz;
    vNormal = n;
    vColor = aColor;
    gl_Position = uViewProj * w;
}
)";

const char* kMeshFS = R"(#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec3 vColor;
uniform vec3 uCamPos;
uniform float uReflection;
uniform float uGloss;
out vec4 fragColor;
void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorld);
    if (dot(N, V) < 0.0) N = -N;               // two-sided lighting

    vec3 keyDir  = normalize(vec3(0.45, 0.85, 0.35));
    vec3 fillDir = normalize(vec3(-0.6, 0.35, -0.55));
    float key  = max(dot(N, keyDir), 0.0);
    float fill = max(dot(N, fillDir), 0.0);
    float hemi = 0.5 + 0.5 * N.y;
    vec3 ambient = mix(vec3(0.04, 0.05, 0.07), vec3(0.20, 0.24, 0.31), hemi);

    vec3 H = normalize(keyDir + V);
    float spec = pow(max(dot(N, H), 0.0), 72.0) * uGloss * 1.4;
    vec3 R = reflect(-V, N);
    float envStreak = smoothstep(0.55, 0.95, R.y) * 0.18 * uGloss;   // soft studio softbox
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0);

    vec3 c = vColor * (ambient + key * vec3(1.0, 0.96, 0.9) + fill * vec3(0.35, 0.5, 0.85) * 0.45)
           + vec3(spec + envStreak)
           + rim * vec3(0.25, 0.55, 1.0) * 0.35;

    // Contact darkening close to the ground.
    c *= mix(0.55, 1.0, smoothstep(0.0, 0.22, abs(vWorld.y)));

    if (uReflection > 0.5)
        c *= 0.32 * exp(-abs(vWorld.y) * 2.5);

    c = vec3(1.0) - exp(-c * 1.35);            // filmic-ish tone curve
    fragColor = vec4(c, 1.0);
}
)";

const char* kFloorVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uViewProj;
out vec3 vWorld;
void main() {
    vWorld = aPos;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

const char* kFloorFS = R"(#version 330 core
in vec3 vWorld;
uniform vec2 uHalfExtents;
uniform float uTime;
uniform float uScroll;
out vec4 fragColor;
float grid(vec2 p, float spacing, float width) {
    vec2 q = p / spacing;
    vec2 g = abs(fract(q - 0.5) - 0.5) / fwidth(q);
    return 1.0 - min(min(g.x, g.y) / width, 1.0);
}
void main() {
    float r = length(vWorld.xz);
    float fade = 1.0 - smoothstep(1.6, 4.4, r);

    // Grid lines scroll with the car's speed so the car appears to drive.
    vec2 p = vWorld.xz + vec2(0.0, uScroll);
    vec3 col = vec3(0.022, 0.035, 0.058);
    col += vec3(0.07, 0.15, 0.27) * grid(p, 0.5, 1.0) * 0.55;
    col += vec3(0.12, 0.32, 0.60) * grid(p, 2.0, 1.4) * 0.55;

    // Soft shadow under the car (rounded-rectangle distance field).
    vec2 q = abs(vWorld.xz) - uHalfExtents;
    float sd = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
    float shadow = 1.0 - smoothstep(-0.3, 0.45, sd);
    col *= 1.0 - 0.8 * shadow;

    // Pulsing accent ring.
    float ring = exp(-pow((r - 2.55) * 9.0, 2.0)) * (0.28 + 0.07 * sin(uTime * 1.5));
    col += vec3(0.20, 0.55, 1.0) * ring;

    fragColor = vec4(col, fade * (0.80 + 0.18 * shadow));
}
)";

const char* kBackgroundVS = R"(#version 330 core
out vec2 vUV;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char* kBackgroundFS = R"(#version 330 core
in vec2 vUV;
uniform vec2 uResolution;
uniform vec2 uCenter;
out vec4 fragColor;
void main() {
    vec3 top = vec3(0.030, 0.050, 0.085);
    vec3 bottom = vec3(0.006, 0.009, 0.016);
    vec3 col = mix(bottom, top, vUV.y);
    vec2 px = vUV * uResolution;
    float d = length((px - uCenter) / uResolution.y);
    col += vec3(0.05, 0.11, 0.21) * exp(-d * d * 5.0);
    float vignette = smoothstep(1.25, 0.35, length(vUV - 0.5));
    fragColor = vec4(col * mix(0.6, 1.0, vignette), 1.0);
}
)";

GLuint CompileShader(GLenum type, const char* src, std::string* error)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        if (error) *error = std::string("Shader compile error:\n") + log;
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint LinkProgram(const char* vs, const char* fs, std::string* error)
{
    GLuint v = CompileShader(GL_VERTEX_SHADER, vs, error);
    if (!v) return 0;
    GLuint f = CompileShader(GL_FRAGMENT_SHADER, fs, error);
    if (!f) { glDeleteShader(v); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        if (error) *error = std::string("Shader link error:\n") + log;
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

void SetMat4(GLuint p, const char* name, const glm::mat4& m)
{
    glUniformMatrix4fv(glGetUniformLocation(p, name), 1, GL_FALSE, glm::value_ptr(m));
}

// ---------------------------------------------------------------------------
// Procedural placeholder parts
// ---------------------------------------------------------------------------
const glm::vec3 kBodyWhite(0.86f, 0.88f, 0.91f);
const glm::vec3 kAccentBlue(0.08f, 0.40f, 0.95f);
const glm::vec3 kCarbon(0.07f, 0.075f, 0.085f);
const glm::vec3 kDarkMetal(0.18f, 0.19f, 0.21f);
const glm::vec3 kCockpit(0.015f, 0.018f, 0.022f);

glm::mat4 BoxXf(glm::vec3 center, glm::vec3 size, float rotXDeg = 0.0f)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), center);
    m = glm::rotate(m, glm::radians(rotXDeg), glm::vec3(1, 0, 0));
    return glm::scale(m, size);
}

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

// Wheel centred at the origin, axle along X, outer (rim) face towards +X.
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

// Steering wheel in the XY plane, facing the driver (-Z), 12 o'clock = +Y.
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

} // namespace

// ---------------------------------------------------------------------------
// CarScene
// ---------------------------------------------------------------------------
CarScene::CarScene() = default;

CarScene::~CarScene()
{
    if (meshProgram_) glDeleteProgram(meshProgram_);
    if (floorProgram_) glDeleteProgram(floorProgram_);
    if (bgProgram_) glDeleteProgram(bgProgram_);
    if (emptyVao_) glDeleteVertexArrays(1, &emptyVao_);
}

bool CarScene::init(const std::string& modelDir, std::string* error)
{
    meshProgram_ = LinkProgram(kMeshVS, kMeshFS, error);
    floorProgram_ = LinkProgram(kFloorVS, kFloorFS, error);
    bgProgram_ = LinkProgram(kBackgroundVS, kBackgroundFS, error);
    if (!meshProgram_ || !floorProgram_ || !bgProgram_) return false;
    glGenVertexArrays(1, &emptyVao_);

    MeshBuilder body, wheel, sw;
    placeholderBody_ = !LoadObj(modelDir + "/body.obj", body, kBodyWhite);
    if (placeholderBody_) BuildPlaceholderBody(body, geo_);
    if (!LoadObj(modelDir + "/wheel.obj", wheel, glm::vec3(0.1f))) BuildPlaceholderWheel(wheel, kModelWheelRadius);
    if (!LoadObj(modelDir + "/steering_wheel.obj", sw, kCarbon)) BuildPlaceholderSteeringWheel(sw);
    body_.upload(body);
    wheel_.upload(wheel);
    steeringWheel_.upload(sw);

    MeshBuilder floor;
    const float e = 6.0f;
    floor.addQuad({-e, 0, -e}, {-e, 0, e}, {e, 0, e}, {e, 0, -e}, glm::vec3(1));
    floor_.upload(floor);
    return true;
}

void CarScene::update(const TelemetryFrame& f, float dt)
{
    wheelRadius_ = std::max(0.05f, f.settings.wheelSizeMm / 2000.0f);
    const float speed = f.speedKmh / 3.6f;
    wheelSpin_ = std::fmod(wheelSpin_ + speed / wheelRadius_ * dt, 2.0f * kPi);

    // Positive steering = turning right; the car's right side is -X.
    steeringWheelAngle_ = glm::radians(f.steeringDeg);
    roadWheelAngle_ = -glm::radians(f.steeringDeg / geo_.steeringRatio);

    if (!dragging_) {
        yaw_ += yawVelocity_ * dt;
        yawVelocity_ *= std::exp(-3.5f * dt);
    }
    floorScroll_ = std::fmod(floorScroll_ + speed * dt, 2.0f);
}

void CarScene::drag(float dxPixels, float dt)
{
    const float delta = -dxPixels * 0.008f;
    yaw_ += delta;
    dragging_ = true;
    if (dt > 0.0f) yawVelocity_ = glm::mix(yawVelocity_, delta / dt, 0.5f);
}

glm::mat4 CarScene::wheelTransform(int i) const
{
    const bool front = i < 2;
    const bool left = (i % 2) == 0;
    const float track = front ? geo_.frontTrack : geo_.rearTrack;
    const float z = front ? geo_.frontAxleZ : geo_.frontAxleZ - geo_.wheelbase;
    glm::mat4 m = glm::translate(glm::mat4(1.0f), {left ? track * 0.5f : -track * 0.5f, wheelRadius_, z});
    if (front) m = glm::rotate(m, roadWheelAngle_, glm::vec3(0, 1, 0));
    m = glm::rotate(m, wheelSpin_, glm::vec3(1, 0, 0));
    if (!left) m = glm::rotate(m, kPi, glm::vec3(0, 1, 0));   // mirror so the rim faces outwards
    return glm::scale(m, glm::vec3(wheelRadius_ / kModelWheelRadius));   // follow the wheel-size setting
}

glm::mat4 CarScene::steeringWheelTransform() const
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), geo_.steeringWheelPos);
    m = glm::rotate(m, glm::radians(geo_.steeringWheelTiltDeg), glm::vec3(1, 0, 0));
    return glm::rotate(m, steeringWheelAngle_, glm::vec3(0, 0, 1));
}

void CarScene::drawCar(unsigned p, bool reflection)
{
    glUniform1f(glGetUniformLocation(p, "uMirror"), reflection ? -1.0f : 1.0f);
    glUniform1f(glGetUniformLocation(p, "uReflection"), reflection ? 1.0f : 0.0f);
    const GLint gloss = glGetUniformLocation(p, "uGloss");

    glUniform1f(gloss, 1.0f);
    SetMat4(p, "uModel", glm::mat4(1.0f));
    body_.draw();

    glUniform1f(gloss, 0.35f);
    for (int i = 0; i < 4; ++i) {
        SetMat4(p, "uModel", wheelTransform(i));
        wheel_.draw();
    }

    glUniform1f(gloss, 0.6f);
    SetMat4(p, "uModel", steeringWheelTransform());
    steeringWheel_.draw();
}

void CarScene::render(int fbW, int fbH, SceneViewport vp, float time)
{
    // Background gradient across the whole window.
    glViewport(0, 0, fbW, fbH);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glUseProgram(bgProgram_);
    glUniform2f(glGetUniformLocation(bgProgram_, "uResolution"), static_cast<float>(fbW), static_cast<float>(fbH));
    glUniform2f(glGetUniformLocation(bgProgram_, "uCenter"), vp.x + vp.w * 0.5f, vp.y + vp.h * 0.45f);
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    if (vp.w <= 0 || vp.h <= 0) return;
    glViewport(vp.x, vp.y, vp.w, vp.h);

    // Orbit camera; pull back when the viewport is narrow so the car fits.
    const float aspect = static_cast<float>(vp.w) / vp.h;
    const float fovY = glm::radians(30.0f);
    const float halfFovX = std::atan(std::tan(fovY * 0.5f) * aspect);
    const float dist = std::max(4.9f, 1.7f / std::tan(halfFovX));
    const float pitch = glm::radians(17.0f);
    const glm::vec3 target(0.0f, 0.42f, -0.05f);
    const glm::vec3 eye = target + dist * glm::vec3(std::cos(pitch) * std::sin(yaw_), std::sin(pitch),
                                                    std::cos(pitch) * std::cos(yaw_));
    const glm::mat4 viewProj =
        glm::perspective(fovY, aspect, 0.1f, 60.0f) * glm::lookAt(eye, target, glm::vec3(0, 1, 0));

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);

    // 1) Mirrored car = reflection in the glossy floor.
    glUseProgram(meshProgram_);
    SetMat4(meshProgram_, "uViewProj", viewProj);
    glUniform3fv(glGetUniformLocation(meshProgram_, "uCamPos"), 1, glm::value_ptr(eye));
    drawCar(meshProgram_, true);

    // 2) Translucent floor over the reflection.
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(floorProgram_);
    SetMat4(floorProgram_, "uViewProj", viewProj);
    glUniform2f(glGetUniformLocation(floorProgram_, "uHalfExtents"), geo_.frontTrack * 0.42f, 1.25f);
    glUniform1f(glGetUniformLocation(floorProgram_, "uTime"), time);
    glUniform1f(glGetUniformLocation(floorProgram_, "uScroll"), floorScroll_);
    floor_.draw();
    glDisable(GL_BLEND);

    // 3) The car itself.
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(meshProgram_);
    drawCar(meshProgram_, false);

    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);
    glViewport(0, 0, fbW, fbH);
}
