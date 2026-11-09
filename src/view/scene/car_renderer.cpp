#include "view/scene/car_renderer.h"

#include "view/gl/gl_loader.h"
#include "view/gl/shader.h"
#include "view/scene/placeholder_car.h"
#include "view/scene/scene_shaders.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

CarRenderer::CarRenderer() = default;

CarRenderer::~CarRenderer()
{
    if (meshProgram_) glDeleteProgram(meshProgram_);
    if (pbrProgram_) glDeleteProgram(pbrProgram_);
    if (floorProgram_) glDeleteProgram(floorProgram_);
    if (bgProgram_) glDeleteProgram(bgProgram_);
    if (emptyVao_) glDeleteVertexArrays(1, &emptyVao_);
}

bool CarRenderer::init(const std::string& modelDir, const CarGeometry& geometry, std::string* error)
{
    meshProgram_ = LinkProgram(shaders::kMeshVS, shaders::kMeshFS, error);
    floorProgram_ = LinkProgram(shaders::kFloorVS, shaders::kFloorFS, error);
    bgProgram_ = LinkProgram(shaders::kBackgroundVS, shaders::kBackgroundFS, error);
    if (!meshProgram_ || !floorProgram_ || !bgProgram_) return false;
    glGenVertexArrays(1, &emptyVao_);

    if (loadGltf(modelDir + "/car.glb")) {
        placeholderBody_ = false;
    } else {
        MeshBuilder body, wheel, sw;
        placeholderBody_ = !LoadObj(modelDir + "/body.obj", body, placeholder::kBodyWhite);
        if (placeholderBody_) BuildPlaceholderBody(body, geometry);
        if (!LoadObj(modelDir + "/wheel.obj", wheel, glm::vec3(0.1f))) BuildPlaceholderWheel(wheel, placeholder::kWheelRadius);
        if (!LoadObj(modelDir + "/steering_wheel.obj", sw, placeholder::kCarbon)) BuildPlaceholderSteeringWheel(sw);
        body_.upload(body);
        wheel_.upload(wheel);
        steeringWheel_.upload(sw);
    }
    if (!modelNote_.empty()) std::fprintf(stderr, "car.glb: %s", modelNote_.c_str());

    MeshBuilder floor;
    const float e = 6.0f;
    floor.addQuad({-e, 0, -e}, {-e, 0, e}, {e, 0, e}, {e, 0, -e}, glm::vec3(1));
    floor_.upload(floor);
    return true;
}

// A broken model never stops the app: the caller falls back and modelNote_ says what happened.
bool CarRenderer::loadGltf(const std::string& path)
{
    if (!std::ifstream(path)) return false;

    pbrProgram_ = LinkProgram(shaders::kPbrVS, shaders::kPbrFS, &modelNote_);
    if (!pbrProgram_) {
        modelNote_ += "\n";
        return false;
    }
    if (!gltf_.load(path, &modelNote_)) return false;

    const GLuint p = pbrProgram_;
    glUseProgram(p);
    const char* const samplers[] = {"uBaseColorTex", "uMetallicRoughnessTex", "uNormalTex", "uOcclusionTex", "uEmissiveTex"};
    for (int i = 0; i < 5; ++i) glUniform1i(glGetUniformLocation(p, samplers[i]), i);
    glUseProgram(0);
    pbr_ = {glGetUniformLocation(p, "uModel"),       glGetUniformLocation(p, "uBaseColor"),
            glGetUniformLocation(p, "uMetallic"),    glGetUniformLocation(p, "uRoughness"),
            glGetUniformLocation(p, "uEmissive"),    glGetUniformLocation(p, "uNormalScale"),
            glGetUniformLocation(p, "uOcclusion"),   glGetUniformLocation(p, "uAlphaCutoff"),
            glGetUniformLocation(p, "uHasNormalMap"), glGetUniformLocation(p, "uUvSet"),
            glGetUniformLocation(p, "uUvXform")};
    return true;
}

bool CarRenderer::measureGeometry(CarGeometry& g) const
{
    if (!gltf_.loaded() || !gltf_.hasPart(CarPart::WheelFL) || !gltf_.hasPart(CarPart::WheelFR) ||
        !gltf_.hasPart(CarPart::WheelRL) || !gltf_.hasPart(CarPart::WheelRR))
        return false;
    const glm::vec3 fl = gltf_.pivot(CarPart::WheelFL), fr = gltf_.pivot(CarPart::WheelFR);
    const glm::vec3 rl = gltf_.pivot(CarPart::WheelRL), rr = gltf_.pivot(CarPart::WheelRR);
    g.frontAxleZ = (fl.z + fr.z) * 0.5f;
    g.wheelbase = g.frontAxleZ - (rl.z + rr.z) * 0.5f;
    g.frontTrack = std::abs(fl.x - fr.x);
    g.rearTrack = std::abs(rl.x - rr.x);
    return true;
}

// car.glb vertices are already in place, so parts only rotate around their pivots.
glm::mat4 CarRenderer::partTransform(const Car& car, CarPart part) const
{
    if (part == CarPart::Body) return glm::mat4(1.0f);
    const glm::vec3 pivot = gltf_.pivot(part);
    return glm::translate(glm::mat4(1.0f), pivot) * car.partRotation(part) * glm::translate(glm::mat4(1.0f), -pivot);
}

void CarRenderer::bindMaterial(const ModelMaterial& m)
{
    const MaterialMap* maps[5] = {&m.baseColor, &m.metallicRoughness, &m.normal, &m.occlusion, &m.emissive};
    glm::mat3 uvXforms[5];
    GLint uvSets[5];
    for (int i = 0; i < 5; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, maps[i]->texture ? maps[i]->texture : gltf_.whiteTexture());
        uvXforms[i] = maps[i]->uvTransform;
        uvSets[i] = maps[i]->uvSet;
    }
    glUniformMatrix3fv(pbr_.uvXform, 5, GL_FALSE, glm::value_ptr(uvXforms[0]));
    glUniform1iv(pbr_.uvSet, 5, uvSets);
    glUniform4fv(pbr_.baseColor, 1, glm::value_ptr(m.baseColorFactor));
    glUniform1f(pbr_.metallic, m.metallicFactor);
    glUniform1f(pbr_.roughness, m.roughnessFactor);
    glUniform3fv(pbr_.emissive, 1, glm::value_ptr(m.emissiveFactor));
    glUniform1f(pbr_.normalScale, m.normalScale);
    glUniform1f(pbr_.occlusion, m.occlusion.texture ? m.occlusionStrength : 0.0f);
    glUniform1f(pbr_.alphaCutoff, m.alphaMode == AlphaMode::Mask ? m.alphaCutoff : -1.0f);
    glUniform1i(pbr_.hasNormalMap, m.normal.texture ? 1 : 0);
}

void CarRenderer::drawGltf(const Car& car, const glm::vec3& eye)
{
    glm::mat4 xforms[kCarPartCount];
    for (int i = 0; i < kCarPartCount; ++i) xforms[i] = partTransform(car, static_cast<CarPart>(i));

    auto draw = [&](const ModelSurface& s) {
        glUniformMatrix4fv(pbr_.model, 1, GL_FALSE, glm::value_ptr(xforms[static_cast<int>(s.part)]));
        bindMaterial(gltf_.material(s.material));
        s.mesh.draw();
    };

    std::vector<const ModelSurface*> transparent;
    for (const ModelSurface& s : gltf_.surfaces()) {
        if (gltf_.material(s.material).alphaMode == AlphaMode::Blend) transparent.push_back(&s);
        else draw(s);
    }
    if (transparent.empty()) return;

    // Transparent surfaces go last, furthest first, and don't hide each other.
    auto distance = [&](const ModelSurface* s) {
        const glm::vec3 d = glm::vec3(xforms[static_cast<int>(s->part)] * glm::vec4(s->center, 1.0f)) - eye;
        return glm::dot(d, d);
    };
    std::sort(transparent.begin(), transparent.end(),
              [&](const ModelSurface* a, const ModelSurface* b) { return distance(a) > distance(b); });
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (const ModelSurface* s : transparent) draw(*s);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void CarRenderer::drawCar(const Car& car, unsigned p, const glm::vec3& eye, bool reflection)
{
    glUniform1f(glGetUniformLocation(p, "uMirror"), reflection ? -1.0f : 1.0f);
    glUniform1f(glGetUniformLocation(p, "uReflection"), reflection ? 1.0f : 0.0f);
    if (gltf_.loaded()) {
        drawGltf(car, eye);
        return;
    }
    const GLint gloss = glGetUniformLocation(p, "uGloss");

    glUniform1f(gloss, 1.0f);
    SetMat4(p, "uModel", glm::mat4(1.0f));
    body_.draw();

    glUniform1f(gloss, 0.35f);
    for (int i = 0; i < 4; ++i) {
        SetMat4(p, "uModel", car.wheelTransform(i, placeholder::kWheelRadius));
        wheel_.draw();
    }

    glUniform1f(gloss, 0.6f);
    SetMat4(p, "uModel", car.steeringWheelTransform());
    steeringWheel_.draw();
}

void CarRenderer::render(const Car& car, const OrbitCamera& camera, int fbW, int fbH, SceneViewport vp, float time)
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
    const CameraMatrices cam = camera.matrices(vp.w, vp.h);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);

    // 1) Mirrored car = reflection in the glossy floor.
    const GLuint carProgram = gltf_.loaded() ? pbrProgram_ : meshProgram_;
    glUseProgram(carProgram);
    SetMat4(carProgram, "uViewProj", cam.viewProj);
    glUniform3fv(glGetUniformLocation(carProgram, "uCamPos"), 1, glm::value_ptr(cam.eye));
    drawCar(car, carProgram, cam.eye, true);

    // 2) Translucent floor over the reflection. The grid scrolls with the distance
    //    travelled (it repeats every 2 m) so the car appears to drive.
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(floorProgram_);
    SetMat4(floorProgram_, "uViewProj", cam.viewProj);
    glUniform2f(glGetUniformLocation(floorProgram_, "uHalfExtents"), car.geometry().frontTrack * 0.42f, 1.25f);
    glUniform1f(glGetUniformLocation(floorProgram_, "uTime"), time);
    glUniform1f(glGetUniformLocation(floorProgram_, "uScroll"), std::fmod(car.pose().distance, 2.0f));
    floor_.draw();
    glDisable(GL_BLEND);

    // 3) The car itself.
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(carProgram);
    drawCar(car, carProgram, cam.eye, false);

    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(0);
    glViewport(0, 0, fbW, fbH);
}
