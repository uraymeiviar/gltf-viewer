// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Readback probes of the agent control channel (probe / probe_region).

#include "CommandsInternal.h"

namespace ux3d::imgui::application {

// ---------------------------------------------------------------------------------------------
// Readback (probe / probe_region): 1x1 or rectangle reads of existing targets on demand only.
// ---------------------------------------------------------------------------------------------

json Application::probePixel(ux3d::imgui::ViewportInstance* vp, int x, int y, bool withPick)
{
    const int width = vp->sceneFbo.getWidth();
    const int height = vp->sceneFbo.getHeight();
    const int glY = height - 1 - y;
    GLint previousRead = 0, previousDraw = 0, previousReadBuffer = GL_BACK, previousPack = 4;
    GLint previousViewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPack);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    float hdr[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->sceneFbo.getResolveFbo());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(x, glY, 1, 1, GL_RGBA, GL_FLOAT, hdr);
    uint8_t ldr[4] = {0, 0, 0, 0};
    const bool haveLdr = vp->overlayFbo.isValid() && vp->overlayFbo.getWidth() == width &&
                         vp->overlayFbo.getHeight() == height;
    if (haveLdr) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->overlayFbo.getResolveFbo());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(x, glY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, ldr);
    }

    // A6.4: report linear radiance (environment units); the scene target holds it times the viewport's pre-exposure.
    if (vp->preExposure != 1.0f) {
        for (int c = 0; c < 3; ++c) hdr[c] /= vp->preExposure;
    }
    json out;
    out["x"] = x;
    out["y"] = y;
    out["viewport"] = vp->id;
    out["hdr"] = {hdr[0], hdr[1], hdr[2], hdr[3]};
    out["hdrLuminance"] = 0.2126 * hdr[0] + 0.7152 * hdr[1] + 0.0722 * hdr[2];
    out["hdrSource"] = vp->sceneFbo.isLinearFloat() ? "scene target, linear RGBA16F before TAA, bloom and tone mapping"
                                                    : "scene target (8-bit fallback, clamped)";
    out["ldr"] = haveLdr ? json{ldr[0], ldr[1], ldr[2], ldr[3]} : json(nullptr);

    if (withPick) {
        json pick = nullptr;
        float depth = 1.0f;
        auto* camera = gltfLayer ? gltfLayer->getActiveCamera() : nullptr;
        if (camera != nullptr && activeScene != nullptr) {
            pick = performPickAtPixel(x, y, vp);
            // A picking id without a glTF mesh behind it is background too.
            if (pick.is_object() && pick.value("nodeIndex", -1) < 0 && pick.value("mesh", std::string()).empty()) {
                pick = nullptr;
            }
            // The picking pass draws with the scene's own depth (only x/y are rescaled), so
            // its depth is the unjittered scene depth at this pixel.
            glBindFramebuffer(GL_READ_FRAMEBUFFER, pickingTarget.getFbo());
            glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        }
        out["pick"] = pick;
        out["depth"] = depth;
        if (camera != nullptr && !pick.is_null() && depth < 1.0f) {
            const glm::mat4 viewProj = glm::mat4(camera->getProjectionMatrix()) * glm::mat4(camera->getViewMatrix());
            const glm::vec4 ndc((static_cast<float>(x) + 0.5f) / static_cast<float>(width) * 2.0f - 1.0f,
                                (static_cast<float>(glY) + 0.5f) / static_cast<float>(height) * 2.0f - 1.0f,
                                depth * 2.0f - 1.0f, 1.0f);
            glm::vec4 world = glm::inverse(viewProj) * ndc;
            world /= world.w;
            out["world"] = {world.x, world.y, world.z};
        } else {
            out["world"] = nullptr;
        }
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousRead));
    glReadBuffer(static_cast<GLenum>(previousReadBuffer));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDraw));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    glPixelStorei(GL_PACK_ALIGNMENT, previousPack);
    return out;
}

json Application::probeRegion(ux3d::imgui::ViewportInstance* vp, int x0, int y0, int x1, int y1)
{
    const int width = vp->sceneFbo.getWidth();
    const int height = vp->sceneFbo.getHeight();
    const int left = std::max(0, std::min(x0, x1));
    const int right = std::min(width, std::max(x0, x1));
    const int top = std::max(0, std::min(y0, y1));
    const int bottom = std::min(height, std::max(y0, y1));
    const int w = right - left;
    const int h = bottom - top;
    if (w <= 0 || h <= 0) return nullptr;

    GLint previousRead = 0, previousReadBuffer = GL_BACK, previousPack = 4;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPack);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    std::vector<float> pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->sceneFbo.getResolveFbo());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(left, height - bottom, w, h, GL_RGBA, GL_FLOAT, pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousRead));
    glReadBuffer(static_cast<GLenum>(previousReadBuffer));
    glPixelStorei(GL_PACK_ALIGNMENT, previousPack);
    if (vp->preExposure != 1.0f) {
        // A6.4: linear radiance without the viewport's pre-exposure.
        for (size_t i = 0; i < pixels.size(); ++i) {
            if (i % 4u != 3u) pixels[i] /= vp->preExposure;
        }
    }

    double minRgb[3] = {1e30, 1e30, 1e30}, maxRgb[3] = {-1e30, -1e30, -1e30}, sumRgb[3] = {0, 0, 0};
    double minLum = 1e30, maxLum = -1e30, sumLum = 0.0;
    const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h);
    for (size_t i = 0; i < count; ++i) {
        const float* p = &pixels[i * 4u];
        for (int c = 0; c < 3; ++c) {
            minRgb[c] = std::min(minRgb[c], static_cast<double>(p[c]));
            maxRgb[c] = std::max(maxRgb[c], static_cast<double>(p[c]));
            sumRgb[c] += p[c];
        }
        const double lum = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
        minLum = std::min(minLum, lum);
        maxLum = std::max(maxLum, lum);
        sumLum += lum;
    }
    const double n = static_cast<double>(count);
    json out;
    out["rect"] = {left, top, right, bottom};
    out["pixels"] = count;
    out["viewport"] = vp->id;
    out["luminance"] = {{"min", minLum}, {"max", maxLum}, {"mean", sumLum / n}};
    out["rgb"] = {{"min", {minRgb[0], minRgb[1], minRgb[2]}},
                  {"max", {maxRgb[0], maxRgb[1], maxRgb[2]}},
                  {"mean", {sumRgb[0] / n, sumRgb[1] / n, sumRgb[2] / n}}};
    out["hdrSource"] = "scene target, linear, before TAA, bloom and tone mapping";
    return out;
}

} // namespace ux3d::imgui::application
