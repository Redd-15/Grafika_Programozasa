// View: GLSL sources for the 3D scene (car, glTF car, floor, background).
#pragma once

namespace shaders {

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------
inline constexpr const char* kMeshVS = R"(#version 330 core
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

inline constexpr const char* kMeshFS = R"(#version 330 core
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

// Textured car from car.glb: metallic-roughness shading under the same studio lights.
inline constexpr const char* kPbrVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV0;
layout(location = 4) in vec2 aUV1;
layout(location = 5) in vec4 aTangent;
uniform mat4 uModel;
uniform mat4 uViewProj;
uniform float uMirror;
out vec3 vWorld;
out vec3 vNormal;
out vec3 vTangent;
out vec3 vBitangent;
out vec3 vColor;
out vec2 vUV0;
out vec2 vUV1;
void main() {
    // uModel only rotates parts around their pivots, so it also transforms directions.
    mat3 rot = mat3(uModel);
    vec3 n = rot * aNormal;
    vec3 t = rot * aTangent.xyz;
    vec3 b = cross(n, t) * aTangent.w;
    vec3 mirror = vec3(1.0, uMirror, 1.0);
    vec4 w = uModel * vec4(aPos, 1.0);
    w.y *= uMirror;
    vWorld = w.xyz;
    vNormal = n * mirror;
    vTangent = t * mirror;
    vBitangent = b * mirror;
    vColor = aColor;
    vUV0 = aUV0;
    vUV1 = aUV1;
    gl_Position = uViewProj * w;
}
)";

inline constexpr const char* kPbrFS = R"(#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec3 vTangent;
in vec3 vBitangent;
in vec3 vColor;
in vec2 vUV0;
in vec2 vUV1;
uniform vec3 uCamPos;
uniform float uReflection;
uniform sampler2D uBaseColorTex;
uniform sampler2D uMetallicRoughnessTex;
uniform sampler2D uNormalTex;
uniform sampler2D uOcclusionTex;
uniform sampler2D uEmissiveTex;
uniform mat3 uUvXform[5];   // one per texture above
uniform int uUvSet[5];
uniform vec4 uBaseColor;
uniform float uMetallic;
uniform float uRoughness;
uniform vec3 uEmissive;
uniform float uNormalScale;
uniform float uOcclusion;
uniform float uAlphaCutoff;  // negative = no alpha test
uniform int uHasNormalMap;
out vec4 fragColor;

const float kPi = 3.14159265;

vec2 SlotUV(int slot) {
    vec2 uv = uUvSet[slot] == 1 ? vUV1 : vUV0;
    return (uUvXform[slot] * vec3(uv, 1.0)).xy;
}

// Falls back to screen-space derivatives for meshes exported without normals or tangents.
vec3 ShadingNormal() {
    vec3 dp1 = dFdx(vWorld);
    vec3 dp2 = dFdy(vWorld);
    vec3 N = dot(vNormal, vNormal) > 1e-6 ? vNormal : cross(dp1, dp2);
    N = normalize(N);
    if (uHasNormalMap == 0) return N;

    vec2 uv = SlotUV(2);
    vec3 T = vTangent;
    vec3 B = vBitangent;
    if (dot(T, T) < 1e-6) {
        vec2 duv1 = dFdx(uv);
        vec2 duv2 = dFdy(uv);
        T = (duv2.y * dp1 - duv1.y * dp2) * sign(duv1.x * duv2.y - duv2.x * duv1.y);
        T -= N * dot(N, T);
        if (dot(T, T) < 1e-12) return N;
        T = normalize(T);
        B = cross(N, T);
    }
    vec3 m = texture(uNormalTex, uv).xyz * 2.0 - 1.0;
    m.xy *= uNormalScale;
    return normalize(T * m.x + B * m.y + N * m.z);
}

// What shiny surfaces reflect: a floor-to-ceiling gradient like the background, an overhead
// softbox and a blue strip light along each side. Rougher surfaces see it more blurred.
vec3 Studio(vec3 d, float roughness) {
    float spread = 0.05 + roughness * roughness * 0.9;
    float dim = 1.0 / (1.0 + 3.0 * roughness * roughness);
    vec3 c = mix(vec3(0.012, 0.016, 0.024), vec3(0.12, 0.15, 0.20), smoothstep(-0.4 - spread, 0.4 + spread, d.y));
    c += vec3(1.6, 1.55, 1.5) * smoothstep(0.62 - spread, 0.82 + spread, d.y) * dim;
    float strip = (1.0 - smoothstep(0.0, 0.35 + spread, abs(d.y - 0.18))) * smoothstep(0.25 - spread, 0.7 + spread, abs(d.x));
    c += vec3(0.45, 0.62, 0.95) * strip * dim;
    return c;
}

// Share of the environment a surface reflects (Karis' fit of the split-sum BRDF).
vec3 EnvironmentBrdf(vec3 F0, float roughness, float NoV) {
    vec4 r = roughness * vec4(-1.0, -0.0275, -0.572, 0.022) + vec4(1.0, 0.0425, 1.04, -0.04);
    float a004 = min(r.x * r.x, pow(2.0, -9.28 * NoV)) * r.x + r.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
    return F0 * ab.x + ab.y;
}

vec3 DirectLight(vec3 L, vec3 radiance, vec3 N, vec3 V, vec3 diffuse, vec3 F0, float alpha) {
    float NoL = dot(N, L);
    if (NoL <= 0.0) return vec3(0.0);
    vec3 H = normalize(L + V);
    float NoV = max(dot(N, V), 1e-3);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);
    float a2 = alpha * alpha;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    float D = a2 / (kPi * d * d);
    float G = 0.5 / (NoL * sqrt(NoV * NoV * (1.0 - a2) + a2) + NoV * sqrt(NoL * NoL * (1.0 - a2) + a2));
    vec3 F = F0 + (vec3(1.0) - F0) * pow(1.0 - VoH, 5.0);
    return (diffuse * (vec3(1.0) - F) / kPi + vec3(D * G) * F) * radiance * NoL;
}

void main() {
    vec4 base = texture(uBaseColorTex, SlotUV(0)) * uBaseColor;
    base.rgb *= vColor;
    if (base.a < uAlphaCutoff) discard;

    vec2 mr = texture(uMetallicRoughnessTex, SlotUV(1)).bg;   // glTF: metallic in B, roughness in G
    float metallic = clamp(mr.x * uMetallic, 0.0, 1.0);
    float roughness = clamp(mr.y * uRoughness, 0.04, 1.0);
    float ao = mix(1.0, texture(uOcclusionTex, SlotUV(3)).r, uOcclusion);
    vec3 emissive = texture(uEmissiveTex, SlotUV(4)).rgb * uEmissive;

    vec3 V = normalize(uCamPos - vWorld);
    vec3 N = ShadingNormal();
    if (dot(N, V) < 0.0) N = -N;               // two-sided lighting
    float NoV = max(dot(N, V), 1e-3);

    vec3 diffuse = base.rgb * (1.0 - metallic);
    vec3 F0 = mix(vec3(0.04), base.rgb, metallic);
    float alpha = roughness * roughness;

    vec3 c = DirectLight(normalize(vec3(0.45, 0.85, 0.35)), vec3(3.0, 2.88, 2.7), N, V, diffuse, F0, alpha)
           + DirectLight(normalize(vec3(-0.6, 0.35, -0.55)), vec3(0.45, 0.65, 1.1), N, V, diffuse, F0, alpha);
    vec3 ambient = mix(vec3(0.008, 0.010, 0.016), vec3(0.16, 0.19, 0.25), 0.5 + 0.5 * N.y);
    c += (diffuse * ambient + Studio(reflect(-V, N), roughness) * EnvironmentBrdf(F0, roughness, NoV)) * ao;
    c += emissive;

    // Contact darkening close to the ground.
    c *= mix(0.55, 1.0, smoothstep(0.0, 0.22, abs(vWorld.y)));

    if (uReflection > 0.5)
        c *= 0.32 * exp(-abs(vWorld.y) * 2.5);

    c = vec3(1.0) - exp(-c * 1.35);            // filmic-ish tone curve
    fragColor = vec4(pow(c, vec3(1.0 / 2.2)), base.a);
}
)";

inline constexpr const char* kFloorVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uViewProj;
out vec3 vWorld;
void main() {
    vWorld = aPos;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

inline constexpr const char* kFloorFS = R"(#version 330 core
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

inline constexpr const char* kBackgroundVS = R"(#version 330 core
out vec2 vUV;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

inline constexpr const char* kBackgroundFS = R"(#version 330 core
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

} // namespace shaders
