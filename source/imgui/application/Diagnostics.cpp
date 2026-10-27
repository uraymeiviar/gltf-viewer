// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "imgui/RenderQuality.h"
#include "imgui/GlCaps.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/stb_image_write.h"
#include "imgui/SceneHierarchyPanel.h"
#include "renderer/Skin.h"
#include "gltf/Material.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

namespace ux3d::imgui::application {
namespace {
std::string effectiveAAName(const ViewportSettingsState& settings)
{
    const char* msaaNames[] = { "none", "msaa2", "msaa4", "msaa8" };
    const char* postNames[] = { "none", "fxaa", "taa" };
    const int msaa = settings.effectiveMsaaIdx();
    const int post = settings.effectivePostAAIdx();
    if (msaa == 0) return postNames[post];
    if (post == 0) return msaaNames[msaa];
    return std::string(msaaNames[msaa]) + "+" + postNames[post];
}
}


int Application::writeSelfCheckReport(const ux3d::imgui::SelfCheckResult& result, const std::string& path,
                                      const char* label)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        std::fprintf(stderr, "Could not open %s self-check output '%s'\n", label, path.c_str());
        return 2;
    }

    output << result.report;
    output.flush();
    if (!output.good()) {
        std::fprintf(stderr, "Could not write %s self-check output '%s'\n", label, path.c_str());
        return 2;
    }
    output.close();
    if (!output.good()) {
        std::fprintf(stderr, "Could not close %s self-check output '%s'\n", label, path.c_str());
        return 2;
    }

    if (!result.success) {
        std::fprintf(stderr, "%s self-check failed; report: %s\n", label, path.c_str());
        return 1;
    }
    return 0;
}

int Application::drainGLErrors(const char* where)
{
    int count = 0;
    GLenum error = GL_NO_ERROR;
    while ((error = glGetError()) != GL_NO_ERROR) {
        ++count;
        ++s_glErrorTotal;
        std::fprintf(stderr, "GL error 0x%X during %s\n", error, where != nullptr ? where : "automation");
    }
    return count;
}

bool Application::writeDecalPrepassCheck(const std::string& path, ux3d::imgui::ViewportInstance& viewport)
{
    const int width = s_decalCheckCaptured ? s_decalCheckWidth : viewport.sceneFbo.getWidth();
    const int height = s_decalCheckCaptured ? s_decalCheckHeight : viewport.sceneFbo.getHeight();
    const bool prepassValid = decalBuffer.isValid();
    const size_t expectedPixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    const bool forwardCaptured = s_decalCheckCaptured && width > 0 && height > 0
        && s_decalCheckForwardColor.size() == expectedPixels * 4u
        && s_decalCheckForwardDepth.size() == expectedPixels;
    const bool colorValid = forwardCaptured;

    std::vector<float> prepassDepth;
    std::vector<float> g0;
    const std::vector<float>& forwardDepth = s_decalCheckForwardDepth;
    const std::vector<uint8_t>& forwardColor = s_decalCheckForwardColor;
    bool depthCompared = false;
    bool normalCompared = false;
    bool readSucceeded = true;
    const bool singleSampleDepth = forwardCaptured;

    GLint previousPackAlignment = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    drainGLErrors("decal-prepass-check-before");

    if (prepassValid && colorValid) {
        const size_t pixelCount = expectedPixels;

        prepassDepth.resize(pixelCount);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, decalBuffer.getPrepassFbo());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, prepassDepth.data());
        if (drainGLErrors("decal-prepass-check-prepass-depth") != 0) {
            readSucceeded = false;
        }

        // G0's ten-bit normal is read as normalized floats, then quantized exactly like
        // the "Decal prepass normal" debug view quantizes it into the RGBA8 viewport.
        g0.resize(pixelCount * 4u);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, g0.data());
        if (drainGLErrors("decal-prepass-check-g0") != 0) {
            readSucceeded = false;
        }

        const bool geometryNormalView =
            viewport.settings.renderMode == ux3d::renderer::RenderMode::NormalGeometry;

        uint64_t depthComparedPixels = 0;
        uint64_t depthExactPixels = 0;
        uint64_t depthOverOneUlp = 0;
        double depthMaxAbsDiff = 0.0;
        double prepassMinDepth = 1.0;
        double forwardMinDepth = 1.0;
        uint64_t prepassBelowOne = 0;
        uint64_t forwardBelowOne = 0;
        uint64_t normalComparedPixels = 0;
        uint64_t normalExactPixels = 0;
        uint64_t normalOverOne512 = 0;
        int normalMaxChannelDiff8 = 0;
        double normalMaxAbsDiff = 0.0;
        double normalMaxComponentDiff = 0.0;

        // The comparison covers receiver geometry only: both targets must hold a depth
        // below the clear value. That leaves out the skybox (which the prepass excludes)
        // and anything the forward pass leaves at the far plane.
        constexpr float kFloatUlp = 1.1920928955078125e-7f; // one ulp at 1.0f
        constexpr double kOneOver512 = 1.0 / 512.0;
        for (size_t i = 0; i < pixelCount; ++i) {
            prepassMinDepth = std::min(prepassMinDepth, static_cast<double>(prepassDepth[i]));
            if (prepassDepth[i] < 1.0f) {
                ++prepassBelowOne;
            }
            if (singleSampleDepth && forwardDepth[i] < 1.0f) {
                ++forwardBelowOne;
            }
            if (singleSampleDepth) {
                forwardMinDepth = std::min(forwardMinDepth, static_cast<double>(forwardDepth[i]));
            }
            const bool geometry = prepassDepth[i] < 1.0f
                && (!singleSampleDepth || forwardDepth[i] < 1.0f);
            if (!geometry) {
                continue;
            }

            if (singleSampleDepth) {
                ++depthComparedPixels;
                const double diff = std::fabs(static_cast<double>(prepassDepth[i]) - forwardDepth[i]);
                depthMaxAbsDiff = std::max(depthMaxAbsDiff, diff);
                if (diff == 0.0) {
                    ++depthExactPixels;
                }
                if (diff > kFloatUlp) {
                    ++depthOverOneUlp;
                }
            }

            if (geometryNormalView) {
                ++normalComparedPixels;
                bool normalExact = true;
                for (int c = 0; c < 3; ++c) {
                    const float prepassEncoded = std::clamp(g0[i * 4u + c], 0.0f, 1.0f);
                    const int prepassByte = static_cast<int>(std::lround(prepassEncoded * 255.0f));
                    const int forwardByte = static_cast<int>(forwardColor[i * 4u + c]);
                    const int channelDiff = std::abs(prepassByte - forwardByte);
                    const double encodedDiff = static_cast<double>(channelDiff) / 255.0;
                    normalMaxChannelDiff8 = std::max(normalMaxChannelDiff8, channelDiff);
                    normalMaxAbsDiff = std::max(normalMaxAbsDiff, encodedDiff);
                    const double componentDiff = std::fabs(
                        static_cast<double>(prepassEncoded * 2.0f - 1.0f)
                        - static_cast<double>(static_cast<float>(forwardByte) / 255.0f * 2.0f - 1.0f));
                    normalMaxComponentDiff = std::max(normalMaxComponentDiff, componentDiff);
                    if (channelDiff != 0) {
                        normalExact = false;
                    }
                    if (encodedDiff > kOneOver512) {
                        ++normalOverOne512;
                    }
                }
                if (normalExact) {
                    ++normalExactPixels;
                }
            }
        }

        depthCompared = singleSampleDepth && depthComparedPixels > 0;
        normalCompared = geometryNormalView && normalComparedPixels > 0;

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) {
            std::fprintf(stderr, "Could not open decal prepass check output '%s'\n", path.c_str());
            glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
            return false;
        }
        output << std::setprecision(10);
        output << "{\n";
        output << "  \"schema\": \"glider.decal-prepass-check.v1\",\n";
        output << "  \"width\": " << width << ",\n";
        output << "  \"height\": " << height << ",\n";
        output << "  \"decalBuffer\": {\"active\": " << (prepassValid ? "true" : "false")
               << ", \"width\": " << decalBuffer.getWidth() << ", \"height\": " << decalBuffer.getHeight()
               << ", \"bytes\": " << decalBuffer.getMemoryBytes()
               << ", \"allocations\": " << decalBuffer.getAllocationCount()
               << ", \"resizes\": " << decalBuffer.getResizeCount()
               << ", \"releases\": " << decalBuffer.getReleaseCount() << "},\n";
        output << "  \"depth\": {\"compared\": " << (depthCompared ? "true" : "false")
               << ", \"pixels\": " << depthComparedPixels
               << ", \"exactPixels\": " << depthExactPixels
               << ", \"overOneUlp\": " << depthOverOneUlp
               << ", \"prepassBelowOne\": " << prepassBelowOne
               << ", \"forwardBelowOne\": " << forwardBelowOne
               << ", \"prepassMin\": " << prepassMinDepth
               << ", \"forwardMin\": " << forwardMinDepth
               << ", \"maxAbsDiff\": " << depthMaxAbsDiff << "},\n";
        output << "  \"normal\": {\"compared\": " << (normalCompared ? "true" : "false")
               << ", \"pixels\": " << normalComparedPixels
               << ", \"exactPixels\": " << normalExactPixels
               << ", \"overOne512\": " << normalOverOne512
               << ", \"maxChannelDiff8\": " << normalMaxChannelDiff8
               << ", \"maxEncodedDiff\": " << normalMaxAbsDiff
               << ", \"maxNormalComponentDiff\": " << normalMaxComponentDiff << "},\n";
        output << "  \"glErrors\": " << s_glErrorTotal << ",\n";
        output << "  \"readSucceeded\": " << (readSucceeded ? "true" : "false") << "\n";
        output << "}\n";
        output.flush();
        if (!output.good()) {
            std::fprintf(stderr, "Could not write decal prepass check output '%s'\n", path.c_str());
            glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
            return false;
        }
    } else {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) {
            std::fprintf(stderr, "Could not open decal prepass check output '%s'\n", path.c_str());
            glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
            return false;
        }
        output << "{\n  \"schema\": \"glider.decal-prepass-check.v1\",\n";
        output << "  \"width\": " << width << ",\n  \"height\": " << height << ",\n";
        output << "  \"decalBuffer\": {\"active\": " << (prepassValid ? "true" : "false")
               << ", \"bytes\": " << decalBuffer.getMemoryBytes() << "},\n";
        output << "  \"depth\": {\"compared\": false},\n";
        output << "  \"normal\": {\"compared\": false},\n";
        output << "  \"glErrors\": " << s_glErrorTotal << ",\n";
        output << "  \"readSucceeded\": false\n}\n";
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    std::printf("Wrote decal prepass check to %s (%dx%d, depth %s, normal %s)\n", path.c_str(), width, height,
                depthCompared ? "compared" : "skipped", normalCompared ? "compared" : "skipped");
    // An inactive buffer is a valid result (files without decals never create it); the
    // report says so. A missing forward capture is a real failure.
    return readSucceeded && colorValid;
}

bool Application::saveImage(const char* filename, int width, int height, const uint8_t* rgba)
{
    if (!filename || width <= 0 || height <= 0 || !rgba) return false;

    // OpenGL glReadPixels returns bottom-to-top image; flip vertically to save right-side up
    stbi_flip_vertically_on_write(1);

    std::string fn = filename;
    std::string ext = "";
    auto dotPos = fn.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = fn.substr(dotPos);
        for (char& c : ext)
            c = static_cast<char>(std::tolower(c));
    }

    if (ext == ".jpg" || ext == ".jpeg") {
        return stbi_write_jpg(filename, width, height, 4, rgba, 90) != 0;
    }
    if (ext == ".png") {
        return stbi_write_png(filename, width, height, 4, rgba, width * 4) != 0;
    }
    return stbi_write_bmp(filename, width, height, 4, rgba) != 0;
}

uint64_t Application::hashBytesFnv1a64(const uint8_t* data, size_t size)
{
    uint64_t value = 14695981039346656037ULL;
    for (size_t i = 0; i < size; ++i) {
        value ^= static_cast<uint64_t>(data[i]);
        value *= 1099511628211ULL;
    }
    return value;
}

bool Application::captureViewportTexture(GLuint texture, int width, int height, const std::string& path,
                                         const std::string& region, uint64_t frame, bool firstFrame, double timeSeconds,
                                         ux3d::imgui::profiling::CaptureRecord& record)
{
    record = ux3d::imgui::profiling::CaptureRecord();
    record.firstFrame = firstFrame;
    record.source = "viewport-texture";
    record.region = region;
    record.path = path;
    record.frame = frame;
    record.width = width;
    record.height = height;
    record.timeSeconds = timeSeconds;

    if (texture == 0 || width <= 0 || height <= 0 || path.empty()) {
        std::fprintf(stderr, "Capture skipped: invalid texture, size or path\n");
        return false;
    }

    const std::filesystem::path outputPath = std::filesystem::u8path(path);
    if (outputPath.has_parent_path()) {
        std::error_code directoryError;
        std::filesystem::create_directories(outputPath.parent_path(), directoryError);
    }

    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    GLint previousTexture = 0;
    GLint previousPackAlignment = 4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    while (glGetError() != GL_NO_ERROR) {
    }

    const auto readbackStart = ux3d::imgui::profiling::Clock::now();
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const GLenum readError = glGetError();
    glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    record.readbackMs = ux3d::imgui::profiling::elapsedMs(readbackStart);

    if (readError != GL_NO_ERROR) {
        std::fprintf(stderr, "Capture failed: glGetTexImage error 0x%X\n", readError);
        return false;
    }

    record.rgbaHash = hashBytesFnv1a64(pixels.data(), pixels.size());
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);
    const auto writeStart = ux3d::imgui::profiling::Clock::now();
    const bool wroteImage = saveImage(path.c_str(), width, height, pixels.data());
    record.writeMs = ux3d::imgui::profiling::elapsedMs(writeStart);
    if (!wroteImage) {
        std::fprintf(stderr, "Capture failed: could not write %s\n", path.c_str());
        return false;
    }

    std::error_code sizeError;
    const uint64_t fileBytes = static_cast<uint64_t>(std::filesystem::file_size(outputPath, sizeError));
    if (sizeError) {
        std::fprintf(stderr, "Capture failed: could not read output size for %s\n", path.c_str());
        return false;
    }

    record.fileBytes = fileBytes;
    record.valid = true;
    std::printf("Captured viewport frame %llu to %s (%dx%d, %llu bytes, readback %.3f ms, write %.3f ms)\n",
                static_cast<unsigned long long>(frame), path.c_str(), width, height,
                static_cast<unsigned long long>(fileBytes), record.readbackMs, record.writeMs);
    return true;
}

std::string Application::jsonEscape(const std::string& text)
{
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    for (unsigned char c : text) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned int>(c));
                out += buffer;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    out.push_back('"');
    return out;
}

void Application::writeJsonNumber(std::ostream& out, double value)
{
    if (!std::isfinite(value)) {
        out << "null";
        return;
    }
    out << value;
}

void Application::writeJsonVec3(std::ostream& out, const ux3d::math::vec3& value)
{
    out << "[";
    writeJsonNumber(out, value.x);
    out << ", ";
    writeJsonNumber(out, value.y);
    out << ", ";
    writeJsonNumber(out, value.z);
    out << "]";
}

void Application::writeJsonMat4(std::ostream& out, const ux3d::math::mat4x4& matrix)
{
    out << "[";
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (column != 0 || row != 0) out << ", ";
            writeJsonNumber(out, matrix[column][row]);
        }
    }
    out << "]";
}

bool Application::hashFileFnv1a64(const std::string& path, uint64_t& hash, uint64_t& byteCount)
{
    if (path.empty()) return false;
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    ux3d::filesystem::File file = fs.toFile(path.c_str());
    ux3d::core::Vector<uint8_t> data;
    if (file.load(data) != ux3d::filesystem::FileLoadResult::Success) {
        return false;
    }

    uint64_t value = 14695981039346656037ULL; // FNV-1a 64-bit offset basis
    for (size_t i = 0; i < data.size(); ++i) {
        value ^= static_cast<uint64_t>(data[i]);
        value *= 1099511628211ULL; // FNV prime
    }
    hash = value;
    byteCount = static_cast<uint64_t>(data.size());
    return true;
}

const char* Application::antiAliasingName(int index)
{
    switch (index) {
    case 0:
        return "none";
    case 1:
        return "msaa2";
    case 2:
        return "msaa4";
    case 3:
        return "msaa8";
    case 4:
        return "fxaa";
    case 5:
        return "taa";
    default:
        return "unknown";
    }
}

const char* Application::cameraTypeName(ux3d::renderer::Camera::Type type)
{
    switch (type) {
    case ux3d::renderer::Camera::Type::Perspective:
        return "perspective";
    case ux3d::renderer::Camera::Type::Orthographic:
        return "orthographic";
    case ux3d::renderer::Camera::Type::Custom:
        return "custom";
    default:
        return "unknown";
    }
}

const char* Application::selectionTypeName(ux3d::imgui::SelectionType type)
{
    using ux3d::imgui::SelectionType;
    switch (type) {
    case SelectionType::None:
        return "none";
    case SelectionType::Node:
        return "node";
    case SelectionType::Material:
        return "material";
    case SelectionType::Mesh:
        return "mesh";
    case SelectionType::Camera:
        return "camera";
    case SelectionType::Light:
        return "light";
    case SelectionType::Texture:
        return "texture";
    case SelectionType::Image:
        return "image";
    case SelectionType::Animation:
        return "animation";
    case SelectionType::Skin:
        return "skin";
    case SelectionType::MaterialVariant:
        return "materialVariant";
    case SelectionType::Scene:
        return "scene";
    case SelectionType::Sampler:
        return "sampler";
    case SelectionType::Accessor:
        return "accessor";
    case SelectionType::BufferView:
        return "bufferView";
    case SelectionType::Buffer:
        return "buffer";
    case SelectionType::Viewport:
        return "viewport";
    default:
        return "unknown";
    }
}

const char* Application::antiAliasingModeName(int index)
{
    switch (index) {
    case 0:
        return "none";
    case 1:
        return "msaa2";
    case 2:
        return "msaa4";
    case 3:
        return "msaa8";
    case 4:
        return "fxaa";
    case 5:
        return "taa";
    default:
        return "unknown";
    }
}

bool Application::writeScenarioReport(const std::string& reportPath, const std::string& scenarioName, uint64_t frame,
                                      GLFWwindow* window, const std::string& modelPath, bool modelLoaded,
                                      ux3d::gltf::Gltf& gltfModel, ux3d::gltf::Scene* activeScene,
                                      const ux3d::imgui::ViewportInstance& viewport,
                                      const ux3d::imgui::SceneEnvironmentState& environment,
                                      const std::string& environmentPath, bool environmentLoaded,
                                      const ux3d::imgui::AnimationPlayer& animationPlayer,
                                      const ux3d::imgui::SelectionState& selection)
{
    std::error_code directoryError;
    const std::filesystem::path reportFilePath = std::filesystem::u8path(reportPath);
    if (reportFilePath.has_parent_path()) {
        std::filesystem::create_directories(reportFilePath.parent_path(), directoryError);
    }

    std::ofstream out(reportFilePath, std::ios::binary);
    if (!out.is_open()) return false;

    int clientWidth = 0;
    int clientHeight = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    float contentScaleX = 1.0f;
    float contentScaleY = 1.0f;
    glfwGetWindowSize(window, &clientWidth, &clientHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glfwGetWindowContentScale(window, &contentScaleX, &contentScaleY);

    uint64_t modelHash = 0;
    uint64_t modelBytes = 0;
    const bool modelHashed = hashFileFnv1a64(modelPath, modelHash, modelBytes);

    uint64_t environmentHash = 0;
    uint64_t environmentBytes = 0;
    const bool environmentHashed = hashFileFnv1a64(environmentPath, environmentHash, environmentBytes);

    const ux3d::imgui::ViewportSettingsState& settings = viewport.settings;
    const ux3d::renderer::Camera& camera = viewport.rendererCamera;

    ux3d::gltf::Extends sceneExtends;
    bool hasSceneExtends = false;
    if (activeScene != nullptr && !activeScene->getNodes().empty()) {
        sceneExtends = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
        hasSceneExtends = true;
    }

    const char* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glslVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    out << std::fixed << std::setprecision(9);
    out << "{\n";
    out << "  \"schema\": \"glider.scenario.v1\",\n";
    out << "  \"scenarioName\": " << jsonEscape(scenarioName) << ",\n";
    out << "  \"frame\": " << frame << ",\n";
    out << "  \"isolated\": " << (s_isolatedState ? "true" : "false") << ",\n";
    out << "  \"layout\": " << jsonEscape(s_isolatedState ? "default-forced" : "persisted") << ",\n";
    out << "  \"vsync\": " << (s_vsyncEnabled ? "true" : "false") << ",\n";

    out << "  \"application\": {\n";
    out << "    \"name\": \"Glider\",\n";
    out << "    \"build\": " << jsonEscape(std::string(__DATE__) + " " + __TIME__) << ",\n";
    out << "    \"openglVendor\": " << jsonEscape(glVendor != nullptr ? glVendor : "") << ",\n";
    out << "    \"openglRenderer\": " << jsonEscape(glRenderer != nullptr ? glRenderer : "") << ",\n";
    out << "    \"openglVersion\": " << jsonEscape(glVersion != nullptr ? glVersion : "") << ",\n";
    out << "    \"glslVersion\": " << jsonEscape(glslVersion != nullptr ? glslVersion : "") << "\n";
    out << "  },\n";

    out << "  \"window\": {\n";
    out << "    \"clientWidth\": " << clientWidth << ",\n";
    out << "    \"clientHeight\": " << clientHeight << ",\n";
    out << "    \"framebufferWidth\": " << framebufferWidth << ",\n";
    out << "    \"framebufferHeight\": " << framebufferHeight << ",\n";
    out << "    \"contentScaleX\": ";
    writeJsonNumber(out, contentScaleX);
    out << ",\n";
    out << "    \"contentScaleY\": ";
    writeJsonNumber(out, contentScaleY);
    out << "\n";
    out << "  },\n";

    out << "  \"viewport\": {\n";
    out << "    \"id\": " << viewport.id << ",\n";
    out << "    \"name\": " << jsonEscape(viewport.name) << ",\n";
    out << "    \"width\": " << viewport.sceneFbo.getWidth() << ",\n";
    out << "    \"height\": " << viewport.sceneFbo.getHeight() << ",\n";
    out << "    \"samples\": " << viewport.sceneFbo.getSamples() << ",\n";
    out << "    \"pickingWidth\": " << viewport.pickingFbo.getWidth() << ",\n";
    out << "    \"pickingHeight\": " << viewport.pickingFbo.getHeight() << "\n";
    out << "  },\n";

    out << "  \"model\": {\n";
    out << "    \"path\": " << jsonEscape(modelPath) << ",\n";
    out << "    \"loaded\": " << (modelLoaded ? "true" : "false") << ",\n";
    out << "    \"hashScope\": \"container-file\",\n";
    out << "    \"hashAlgorithm\": \"fnv1a64\",\n";
    out << "    \"bytes\": " << modelBytes << ",\n";
    out << "    \"hash\": ";
    if (modelHashed) {
        out << "\"0x" << std::hex << std::setw(16) << std::setfill('0') << modelHash << std::dec << std::setfill(' ')
            << "\"";
    } else {
        out << "null";
    }
    out << "\n";
    out << "  },\n";

    out << "  \"scene\": {\n";
    out << "    \"nodes\": " << gltfModel.getElements<ux3d::gltf::Node>().size() << ",\n";
    out << "    \"meshes\": " << gltfModel.getElements<ux3d::gltf::Mesh>().size() << ",\n";
    out << "    \"materials\": " << gltfModel.getElements<ux3d::gltf::Material>().size() << ",\n";
    out << "    \"boundsMin\": ";
    if (hasSceneExtends)
        writeJsonVec3(out, ux3d::math::vec3(sceneExtends.min));
    else
        out << "null";
    out << ",\n";
    out << "    \"boundsMax\": ";
    if (hasSceneExtends)
        writeJsonVec3(out, ux3d::math::vec3(sceneExtends.max));
    else
        out << "null";
    out << "\n";
    out << "  },\n";

    out << "  \"camera\": {\n";
    out << "    \"type\": " << jsonEscape(cameraTypeName(camera.getType())) << ",\n";
    out << "    \"eye\": ";
    writeJsonVec3(out, viewport.cameraActor.getEyePosition());
    out << ",\n";
    out << "    \"target\": ";
    writeJsonVec3(out, viewport.cameraActor.getLookTarget());
    out << ",\n";
    out << "    \"up\": ";
    writeJsonVec3(out, ux3d::math::vec3(camera.getUp()));
    out << ",\n";
    out << "    \"yawRadians\": ";
    writeJsonNumber(out, viewport.cameraActor.getRotationX());
    out << ",\n";
    out << "    \"pitchRadians\": ";
    writeJsonNumber(out, viewport.cameraActor.getRotationY());
    out << ",\n";
    out << "    \"zoom\": ";
    writeJsonNumber(out, viewport.cameraActor.getZoom());
    out << ",\n";
    out << "    \"yfovRadians\": ";
    writeJsonNumber(out, camera.getYFoV());
    out << ",\n";
    out << "    \"aspectRatio\": ";
    writeJsonNumber(out, camera.getAspectRatio());
    out << ",\n";
    out << "    \"zNear\": ";
    writeJsonNumber(out, camera.getZNearPlane());
    out << ",\n";
    out << "    \"zFar\": ";
    writeJsonNumber(out, camera.getZFarPlane());
    out << ",\n";
    out << "    \"orthographicScaling\": ";
    writeJsonNumber(out, camera.getOrthographicScaling());
    out << ",\n";
    out << "    \"matrixOrder\": \"column-major\",\n";
    out << "    \"view\": ";
    writeJsonMat4(out, camera.getViewMatrix());
    out << ",\n";
    out << "    \"projection\": ";
    writeJsonMat4(out, camera.getProjectionMatrix());
    out << "\n";
    out << "  },\n";

    out << "  \"environment\": {\n";
    out << "    \"path\": " << jsonEscape(environmentPath) << ",\n";
    out << "    \"loaded\": " << (environmentLoaded ? "true" : "false") << ",\n";
    out << "    \"bytes\": " << environmentBytes << ",\n";
    out << "    \"hash\": ";
    if (environmentHashed) {
        out << "\"0x" << std::hex << std::setw(16) << std::setfill('0') << environmentHash << std::dec
            << std::setfill(' ') << "\"";
    } else {
        out << "null";
    }
    out << ",\n";
    out << "    \"uiPath\": " << jsonEscape(environment.currentHdrPath) << ",\n";
    out << "    \"iblEnabled\": " << (environment.iblEnabled ? "true" : "false") << ",\n";
    out << "    \"rotation\": ";
    writeJsonNumber(out, environment.hdrRotation);
    out << ",\n";
    out << "    \"intensity\": ";
    writeJsonNumber(out, environment.hdrIntensity);
    out << ",\n";
    out << "    \"blur\": ";
    writeJsonNumber(out, environment.hdrBlur);
    out << "\n";
    out << "  },\n";

    out << "  \"settings\": {\n";
    out << "    \"antiAliasing\": " << jsonEscape(effectiveAAName(settings)) << ",\n";
    out << "    \"antiAliasingIndex\": " << settings.antiAliasingIdx << ",\n";
    out << "    \"msaaSamples\": " << (settings.effectiveMsaaIdx() == 0 ? 1 : 1 << settings.effectiveMsaaIdx()) << ",\n";
    out << "    \"postAAIndex\": " << settings.effectivePostAAIdx() << ",\n";
    out << "    \"renderModeIndex\": " << settings.renderModeIdx << ",\n";
    out << "    \"toneMappingIndex\": " << environment.toneMappingIdx << ",\n";
    out << "    \"exposureEV\": ";
    writeJsonNumber(out, environment.exposureEV);
    out << ",\n";
    out << "    \"showBackground\": " << (environment.showBackground ? "true" : "false") << ",\n";
    out << "    \"showGrid\": " << (settings.showGrid ? "true" : "false") << ",\n";
    out << "    \"forceDoubleSided\": " << (settings.forceDoubleSided ? "true" : "false") << ",\n";
    out << "    \"invertWinding\": " << (settings.invertWinding ? "true" : "false") << ",\n";
    out << "    \"wireframeOverlay\": " << (settings.wireframeOverlay ? "true" : "false") << ",\n";
    out << "    \"wireframeColor\": [";
    writeJsonNumber(out, settings.wireframeColor.r);
    out << ", ";
    writeJsonNumber(out, settings.wireframeColor.g);
    out << ", ";
    writeJsonNumber(out, settings.wireframeColor.b);
    out << ", ";
    writeJsonNumber(out, settings.wireframeColor.a);
    out << "],\n";
    out << "    \"wireframeBlend\": " << static_cast<int>(settings.wireframeBlend) << ",\n";
    out << "    \"showSceneBoundingBox\": " << (settings.showSceneBoundingBox ? "true" : "false") << ",\n";
    out << "    \"showSelectedBoundingBox\": " << (settings.showSelectedBoundingBox ? "true" : "false") << ",\n";
    out << "    \"showNodeCoordinateGizmo\": " << (settings.showNodeCoordinateGizmo ? "true" : "false") << ",\n";
    out << "    \"showOrientationGizmo\": " << (settings.showOrientationGizmo ? "true" : "false") << ",\n";
    out << "    \"gizmoAxisConvention\": " << settings.gizmoAxisConvention << ",\n";
    out << "    \"turntable\": " << (settings.turntable ? "true" : "false") << ",\n";
    out << "    \"turntableSpeed\": ";
    writeJsonNumber(out, settings.turntableSpeed);
    out << ",\n";
    out << "    \"punctualLightsEnabled\": " << (environment.punctualLightsEnabled ? "true" : "false") << ",\n";
    out << "    \"clearColor\": [";
    writeJsonNumber(out, environment.clearColor.r);
    out << ", ";
    writeJsonNumber(out, environment.clearColor.g);
    out << ", ";
    writeJsonNumber(out, environment.clearColor.b);
    out << ", ";
    writeJsonNumber(out, environment.clearColor.a);
    out << "]\n";
    out << "  },\n";

    out << "  \"animation\": {\n";
    out << "    \"time\": ";
    writeJsonNumber(out, animationPlayer.getCurrentTime());
    out << ",\n";
    out << "    \"maxDuration\": ";
    writeJsonNumber(out, animationPlayer.getMaxDuration());
    out << ",\n";
    out << "    \"playing\": " << (animationPlayer.isPlaying() ? "true" : "false") << ",\n";
    out << "    \"activeIndex\": " << animationPlayer.getActiveAnimationIndex() << ",\n";
    out << "    \"looping\": " << (animationPlayer.isLooping() ? "true" : "false") << ",\n";
    out << "    \"loopCount\": " << animationPlayer.getLoopCount() << ",\n";
    out << "    \"playbackSpeed\": ";
    writeJsonNumber(out, animationPlayer.getPlaybackSpeed());
    out << "\n";
    out << "  },\n";

    out << "  \"selection\": {\n";
    out << "    \"type\": " << jsonEscape(selectionTypeName(selection.type)) << ",\n";
    out << "    \"viewportId\": " << selection.selectedViewportId << ",\n";
    out << "    \"nodeId\": ";
    if (selection.selectedNode != nullptr)
        out << selection.selectedNode->m_ID;
    else
        out << "null";
    out << ",\n";
    out << "    \"nodeName\": ";
    if (selection.selectedNode != nullptr && selection.selectedNode->hasName()) {
        out << jsonEscape(selection.selectedNode->getName());
    } else {
        out << "null";
    }
    out << "\n";
    out << "  }\n";
    out << "}\n";

    out.flush();
    return out.good();
}

void Application::fillGpuReport(ux3d::imgui::profiling::GpuReport& target, const ux3d::renderer::Renderer* renderer)
{
    target = ux3d::imgui::profiling::GpuReport();
    if (renderer == nullptr) {
        return;
    }

    const auto& profiler = renderer->getGpuProfiler();
    target.enabled = profiler.enabled();
    target.method = "GL_TIME_ELAPSED; polled with GL_QUERY_RESULT_AVAILABLE after >=2 later submissions; no glFinish";
    target.cpuSubmissionStage = "draw.submit (CPU, includes command submission only)";
    target.pendingFrames = profiler.pendingFrameCount();
    target.unavailablePolls = profiler.unavailablePollCount();
    target.droppedFrames = profiler.droppedFrameCount();
    target.completedFrameLimit = profiler.completedFrameLimit();
    target.completedFramesDropped = profiler.completedFrameDroppedCount();

    const auto convertGpuFrame = [](const ux3d::renderer::GpuFrameSample& source) {
        ux3d::imgui::profiling::GpuFrameReport converted;
        converted.sequence = source.sequence;
        converted.frame = source.frame;
        converted.viewport = source.viewport;
        converted.label = source.label;
        for (const auto& pass : source.passes) {
            converted.passes.push_back(
                ux3d::imgui::profiling::GpuPassReport{pass.name, pass.available, pass.milliseconds});
        }
        return converted;
    };

    for (const auto& frame : profiler.completedFrames()) {
        target.completed.push_back(convertGpuFrame(frame));
    }
    const auto pendingGpuFrames = profiler.pendingFrames();
    for (const auto& frame : pendingGpuFrames) {
        target.pending.push_back(convertGpuFrame(frame));
    }
}

std::string Application::describeUiSelection(const ux3d::imgui::SelectionState& selection)
{
    auto nameOf = [](auto* element) -> std::string {
        if (element == nullptr) return "";
        return element->hasName() ? std::string(element->getName().c_str()) : std::string("<unnamed>");
    };
    std::string text = "type=" + std::to_string(static_cast<int>(selection.type));
    text += "|node=" + nameOf(selection.selectedNode);
    text += "|mesh=" + nameOf(selection.selectedMesh);
    text += "|material=" + nameOf(selection.selectedMaterial);
    text += "|variant=" + nameOf(selection.selectedVariant);
    text += "|camera=" + nameOf(selection.selectedCamera);
    text += "|light=" + nameOf(selection.selectedLight);
    text += "|viewport=" + std::to_string(selection.selectedViewportId);
    return text;
}

void Application::finishFrame(FrameContext& frame, int displayW, int displayH)
{
    auto& activeEnvironment = getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
    frame.sample.cpuFrameMs = ux3d::imgui::profiling::elapsedMs(frame.start);
    frame.sample.firstFrame = (frame.number == 1);
    if (s_profilingEnabled) {
        frame.sample.counters = ux3d::imgui::profiling::frameCounters();
    }
    if (frame.sample.firstFrame && (s_profilingEnabled || s_benchmarkEnabled)) {
        s_firstFrameSample = frame.sample;
        s_haveFirstFrameSample = true;
    }
    if (s_profilingEnabled) {
        s_frameSamples.push_back(frame.sample);
        if (s_frameSamples.size() > ux3d::imgui::profiling::kMaxRetainedFrameSamples) {
            s_frameSamples.pop_front();
            ++s_frameSamplesDropped;
        }
    }
    if (s_benchmarkEnabled && frame.number > s_benchmarkWarmupFrames &&
        s_benchmarkSamples.size() < s_benchmarkMeasuredFrames) {
        s_benchmarkSamples.push_back(frame.sample);
    }

    ++frameCount;
    const bool profileCaptureReady = s_captureViewportPath.empty() || s_captureAttempted;

    // A2.1: one-shot diagnostic readback of the prepass depth/G0 against the forward
    // depth and Geometry Normal debug view. It runs after the warm-up frames and exits.
    if (!s_decalPrepassCheckPath.empty() && !s_decalPrepassCheckAttempted
        && frameCount >= SCENARIO_REPORT_FRAME && activeViewport != nullptr && s_decalCheckCaptured) {
        s_decalPrepassCheckAttempted = true;
        if (!writeDecalPrepassCheck(s_decalPrepassCheckPath, *activeViewport)) {
            s_outputFailed = true;
        }
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (!s_decalPrepassCheckPath.empty() && !s_decalPrepassCheckAttempted && frameCount >= 64) {
        // The active viewport never rendered into a single-sample target (for example it
        // was closed or stayed inactive). Do not wait forever for a diagnostic.
        s_decalPrepassCheckAttempted = true;
        s_outputFailed = true;
        std::fprintf(stderr, "Decal prepass check could not capture the active viewport\n");
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    // Automation runs read GL errors instead of letting them go unnoticed.
    if (uiScenario.active() || !s_decalPrepassCheckPath.empty() || !s_profileReportPath.empty()) {
        drainGLErrors("frame");
    }
    const bool profileReportReady = !s_profileReportPath.empty() && !s_profileReportAttempted &&
                                    frameCount >= SCENARIO_REPORT_FRAME && activeViewport != nullptr &&
                                    profileCaptureReady;

    if (!profileReportReady && !s_profileReportPath.empty() && !s_profileReportAttempted &&
        !s_profileReportDeferredLogged && frameCount >= SCENARIO_REPORT_FRAME && !s_captureViewportPath.empty() &&
        !s_captureAttempted) {
        std::printf("Deferring profile report until the requested viewport capture completes\n");
        s_profileReportDeferredLogged = true;
    }

    if (profileReportReady) {
        std::printf("GL automation errors: %llu\n", static_cast<unsigned long long>(s_glErrorTotal));
        s_profileReportAttempted = true;
        ux3d::imgui::profiling::ProfileReport report;
        report.scenarioName = scenarioName;
        report.modelPath = currentFilePath.empty() ? modelToLoad : currentFilePath;
        report.build = std::string(__DATE__) + " " + __TIME__;
        const auto* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const auto* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const auto* glRendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        report.glVersion = glVersion != nullptr ? glVersion : "";
        report.glMajor = ux3d::imgui::glCaps().major;
        report.glMinor = ux3d::imgui::glCaps().minor;
        report.glCompute = ux3d::imgui::glCaps().compute;
        report.glMaxComputeWorkGroupInvocations = ux3d::imgui::glCaps().maxComputeWorkGroupInvocations;
        report.gpuVendor = glVendor != nullptr ? glVendor : "";
        report.gpuRenderer = glRendererName != nullptr ? glRendererName : "";
        report.isolated = s_isolatedState;
        report.instrumentationEnabled = s_profilingEnabled;
        report.vsyncRequested = s_vsyncEnabled;
        glfwGetWindowSize(window, &report.windowWidth, &report.windowHeight);
        report.viewportWidth = activeViewport->sceneFbo.getWidth();
        report.viewportHeight = activeViewport->sceneFbo.getHeight();
        report.viewportSamples = activeViewport->sceneFbo.getSamples();
        report.antiAliasing = effectiveAAName(ux3d::imgui::RenderQuality::effectiveViewport(activeViewport->settings));
        report.startup = ux3d::imgui::profiling::startupProfile();
        report.reportFrame = frameCount;
        report.frameSampleLimit = ux3d::imgui::profiling::kMaxRetainedFrameSamples;
        report.frameSamplesDropped = s_frameSamplesDropped;
        report.frames.assign(s_frameSamples.begin(), s_frameSamples.end());
        fillGpuReport(report.gpu, glRenderer.get());
        report.captureRequested = !s_captureViewportPath.empty();
        report.hasFirstFrameCapture = s_firstFrameCaptureAttempted;
        report.firstFrameCapture = s_firstFrameCaptureRecord;
        report.capture = s_captureRecord;
        report.camera = getCameraJson(activeViewport);
        if (!reportNodeArgs.empty()) {
            for (const auto& spec : reportNodeArgs) {
                auto* node = findNode(spec);
                if (node != nullptr) {
                    report.nodes.push_back(computeNodeScreenRect(node, activeViewport));
                } else {
                    nlohmann::json missing;
                    missing["index"] = -1;
                    missing["name"] = spec;
                    missing["visible"] = false;
                    missing["screenRect"] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    missing["onScreen"] = false;
                    missing["clipped"] = true;
                    missing["behindCamera"] = false;
                    missing["center"] = { 0.0f, 0.0f, 0.0f };
                    report.nodes.push_back(missing);
                }
            }
        }
        if (!reportPickArgs.empty()) {
            for (const auto& coord : reportPickArgs) {
                report.picks.push_back(performPickAtPixel(coord.first, coord.second, activeViewport));
            }
        }
        if (ux3d::imgui::profiling::writeProfileReport(s_profileReportPath, report)) {
            std::printf("Wrote profile report to %s (finalized at frame %llu)\n", s_profileReportPath.c_str(),
                        static_cast<unsigned long long>(frameCount));
        } else {
            s_outputFailed = true;
            std::fprintf(stderr, "Failed to write profile report to %s\n", s_profileReportPath.c_str());
        }
        if (s_exitAfterProfile) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
    if (!scenarioReportPath.empty() && frameCount == SCENARIO_REPORT_FRAME && activeViewport != nullptr) {
        const std::string reportModelPath = currentFilePath.empty() ? modelToLoad : currentFilePath;
        if (writeScenarioReport(scenarioReportPath, scenarioName, frameCount, window, reportModelPath, modelLoaded,
                                gltfModel, activeScene, *activeViewport, activeEnvironment, scenarioEnvironmentPath,
                                scenarioEnvironmentLoaded, animationPlayer, selection)) {
            std::printf("Wrote scenario report to %s\n", scenarioReportPath.c_str());
        } else {
            s_outputFailed = true;
            std::fprintf(stderr, "Failed to write scenario report to %s\n", scenarioReportPath.c_str());
        }
    }

    if (s_benchmarkEnabled && !s_benchmarkReportAttempted && s_benchmarkSamples.size() >= s_benchmarkMeasuredFrames) {
        ux3d::imgui::profiling::BenchmarkReport report;
        report.scenarioName = scenarioName;
        report.modelPath = currentFilePath.empty() ? modelToLoad : currentFilePath;
        report.build = std::string(__DATE__) + " " + __TIME__;
        const auto* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const auto* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const auto* glRendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        report.glVersion = glVersion != nullptr ? glVersion : "";
        report.glMajor = ux3d::imgui::glCaps().major;
        report.glMinor = ux3d::imgui::glCaps().minor;
        report.glCompute = ux3d::imgui::glCaps().compute;
        report.glMaxComputeWorkGroupInvocations = ux3d::imgui::glCaps().maxComputeWorkGroupInvocations;
        report.gpuVendor = glVendor != nullptr ? glVendor : "";
        report.gpuRenderer = glRendererName != nullptr ? glRendererName : "";
        report.isolated = s_isolatedState;
        report.vsyncRequested = s_vsyncEnabled;
        report.detailedInstrumentation = s_profilingEnabled;
        glfwGetWindowSize(window, &report.windowWidth, &report.windowHeight);
        if (activeViewport != nullptr) {
            report.viewportWidth = activeViewport->sceneFbo.getWidth();
            report.viewportHeight = activeViewport->sceneFbo.getHeight();
            report.viewportSamples = activeViewport->sceneFbo.getSamples();
            report.antiAliasing = effectiveAAName(activeViewport->settings);
        }
        report.warmupFrames = s_benchmarkWarmupFrames;
        report.measuredFramesRequested = s_benchmarkMeasuredFrames;
        report.hasFirstFrame = s_haveFirstFrameSample;
        report.firstFrame = s_firstFrameSample;
        report.measuredFrames = s_benchmarkSamples;
        fillGpuReport(report.gpu, glRenderer.get());

        s_benchmarkReportAttempted = true;
        s_benchmarkReportWritten = ux3d::imgui::profiling::writeBenchmarkReport(s_benchmarkOutputPath, report);
        if (s_benchmarkReportWritten) {
            std::printf("Wrote benchmark report to %s (%zu measured frames, warmup %llu, vsync %s)\n",
                        s_benchmarkOutputPath.c_str(), s_benchmarkSamples.size(),
                        static_cast<unsigned long long>(s_benchmarkWarmupFrames), s_vsyncEnabled ? "on" : "off");
        } else {
            s_outputFailed = true;
            std::fprintf(stderr, "Benchmark report write failed after attempt: %s\n", s_benchmarkOutputPath.c_str());
        }
        if (s_exitAfterBenchmark) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    if (dumpFrameCountdown > 0) {
        dumpFrameCountdown--;
        if (dumpFrameCountdown == 0 && !dumpFramePath.empty()) {
            std::vector<uint8_t> pixels(displayW * displayH * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            glReadPixels(0, 0, displayW, displayH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            if (saveImage(dumpFramePath.c_str(), displayW, displayH, pixels.data())) {
                std::printf("Dumped window frame to %s\n", dumpFramePath.c_str());
            } else {
                s_outputFailed = true;
                std::fprintf(stderr, "Failed to dump window frame to %s\n", dumpFramePath.c_str());
            }
            if (!dumpNextFramePath.empty()) {
                dumpFramePath = std::move(dumpNextFramePath);
                dumpNextFramePath.clear();
                dumpFrameCountdown = 1;
            } else if (exitAfterDump) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }
    }

    if (uiScenario.active()) {
        ux3d::imgui::UiScenarioFrame uiFrame;
        uiFrame.selection = describeUiSelection(selection);
        if (activeViewport != nullptr) {
            const auto target = activeViewport->cameraActor.getLookTarget();
            char cameraText[160];
            std::snprintf(cameraText, sizeof(cameraText), "target=%.3f,%.3f,%.3f zoom=%.3f", target.x, target.y,
                          target.z, activeViewport->cameraActor.getZoom());
            uiFrame.camera = cameraText;
        }
        uiFrame.logEntries = s_consoleLogs.size();
        uiFrame.logErrors = s_consoleLogs.counts().errors;
        uiFrame.logWarnings = s_consoleLogs.counts().warnings;
        uiFrame.logInfo = s_consoleLogs.counts().info;
        uiFrame.decalBufferActive = decalBuffer.isValid();
        uiFrame.decalBufferWidth = decalBuffer.getWidth();
        uiFrame.decalBufferHeight = decalBuffer.getHeight();
        uiFrame.decalBufferBytes = decalBuffer.getMemoryBytes();
        uiFrame.decalBufferAllocations = decalBuffer.getAllocationCount();
        uiFrame.decalBufferResizes = decalBuffer.getResizeCount();
        uiFrame.decalBufferReleases = decalBuffer.getReleaseCount();
        uiFrame.glErrors = s_glErrorTotal;
        uiFrame.hierarchyBuildCount = ux3d::imgui::SceneHierarchyPanel::rowModelBuildCount();
        {
            std::string revisionsJson =
                "{\"syncs\":" + std::to_string(sceneSyncCount) +
                ",\"skippedSyncs\":" + std::to_string(skippedSceneSyncCount) +
                ",\"animationApplies\":" + std::to_string(animationOnlySyncCount) +
                ",\"boundsComputed\":" + std::to_string(s_boundsComputedTotal) +
                ",\"boundsHits\":" + std::to_string(s_boundsHitTotal) +
                ",\"overrideApplies\":" + std::to_string(s_overrideApplyTotal) +
                ",\"overrideSkips\":" + std::to_string(s_overrideSkipTotal) +
                ",\"programCacheHits\":" + std::to_string(commandGenerator->getCommandMapping().getProgramCacheHits()) +
                ",\"programCacheMisses\":" +
                std::to_string(commandGenerator->getCommandMapping().getProgramCacheMisses()) +
                ",\"commandCacheHits\":" + std::to_string(commandGenerator->getCommandMapping().getCommandCacheHits()) +
                ",\"commandCacheMisses\":" +
                std::to_string(commandGenerator->getCommandMapping().getCommandCacheMisses()) +
                ",\"transformVisited\":" + std::to_string(ux3d::renderer::TransformHierarchyNode::stats().visited) +
                ",\"transformRecomputed\":" +
                std::to_string(ux3d::renderer::TransformHierarchyNode::stats().recomputed) +
                ",\"weightPushes\":" + std::to_string(ux3d::renderer::TransformHierarchyNode::stats().weightPushes) +
                ",\"jointUploads\":" + std::to_string(ux3d::renderer::Skin::s_jointUploadStats.uploads) +
                ",\"jointUploadsSuppressed\":" + std::to_string(ux3d::renderer::Skin::s_jointUploadStats.suppressed) +
                ",\"pickingRenders\":" + std::to_string(s_pickingRenderTotal) +
                ",\"pickingSkips\":" + std::to_string(s_pickingSkipTotal) +
                ",\"pickReads\":" + std::to_string(s_pickReadTotal) +
                ",\"pickUnserved\":" + std::to_string(s_pickUnservedTotal) +
                ",\"pickCulled\":" + std::to_string(s_pickCulledTotal) +
                ",\"traversalReuses\":" + std::to_string(gltfLayer->getTraversalReuses()) +
                ",\"instancedDraws\":" + std::to_string(s_sceneInstancedDraws) +
                ",\"instancedMembers\":" + std::to_string(s_sceneInstancedMembers) +
                ",\"sceneRenders\":" + std::to_string(s_sceneRenderTotal) +
                ",\"sceneSkips\":" + std::to_string(s_sceneSkipTotal) + ",\"culled\":" + std::to_string(s_culledTotal) +
                ",\"uncullable\":" + std::to_string(s_uncullableTotal) +
                ",\"marks\":" + std::to_string(sceneRevisions.markCount()) + ",\"lastConsumed\":\"" +
                ux3d::imgui::SceneRevisions::maskToString(lastFrameSceneChanges) + "\"" + ",\"pending\":\"" +
                ux3d::imgui::SceneRevisions::maskToString(sceneRevisions.pendingMask()) + "\"";
            for (size_t i = 0; i < ux3d::imgui::kSceneChangeCount; ++i) {
                const auto change = static_cast<ux3d::imgui::SceneChange>(i);
                revisionsJson += ",\"";
                revisionsJson += ux3d::imgui::sceneChangeName(change);
                revisionsJson += "\":" + std::to_string(sceneRevisions.revision(change));
            }
            revisionsJson += "}";
            uiFrame.revisionsJson = revisionsJson;
        }
        uiFrame.expectedCopyText = [&]() {
            std::string expected;
            for (const auto& entry : s_consoleLogs.entries()) {
                expected += entry.text;
                expected += "\n";
            }
            return expected;
        };
        uiFrame.appendLog = [&](const std::string& text, int severity) {
            const auto level = severity == 2   ? ux3d::core::LogLevel::Error
                               : severity == 1 ? ux3d::core::LogLevel::Warning
                                               : ux3d::core::LogLevel::Info;
            const ImVec4 color = severity == 2   ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
                                 : severity == 1 ? ImVec4(1.0f, 0.85f, 0.3f, 1.0f)
                                                 : ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
            s_consoleLogs.append({text, color, level});
        };
        uiFrame.saveFrame = [&](const std::string& path) {
            std::vector<uint8_t> pixels(static_cast<size_t>(displayW) * displayH * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            glReadPixels(0, 0, displayW, displayH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            return saveImage(path.c_str(), displayW, displayH, pixels.data());
        };
        uiFrame.loadModel = [&](const std::string& path) {
            const bool loaded = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports,
                                              gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
            updateWindowTitle(window, currentFilePath);
            return loaded;
        };
        uiFrame.loadModelWithoutTextures = [&](const std::string& path) {
            const bool loaded = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports,
                gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments, false, true);
            updateWindowTitle(window, currentFilePath);
            return loaded;
        };
        uiFrame.pickNode = [&](int id) {
            for (const auto& node : gltfModel.getNodes()) {
                if (static_cast<int>(node.m_ID) == id) {
                    auto* pickedNode = const_cast<ux3d::gltf::Node*>(&node);
                    selection.selectNode(pickedNode);
                    ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
                    sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
                    return true;
                }
            }
            return false;
        };
        for (const auto& vp : viewports) {
            if (!vp) continue;
            ux3d::imgui::UiScenarioFrame::ViewportInfo vinfo;
            vinfo.id = vp->id;
            vinfo.name = vp->name;
            vinfo.open = vp->isOpen;
            vinfo.renderW = vp->sceneFbo.getWidth();
            vinfo.renderH = vp->sceneFbo.getHeight();
            vinfo.aaMode = vp->settings.antiAliasingIdx;
            vinfo.aaW = vp->antiAliasing.getWidth();
            vinfo.aaH = vp->antiAliasing.getHeight();
            vinfo.aaTargets = vp->antiAliasing.getOwnedTargetCount();
            vinfo.aaFbos = vp->antiAliasing.getOwnedFramebufferCount();
            vinfo.aaBytes = vp->antiAliasing.getEstimatedLiveBytes();
            vinfo.aaHistoryValid = vp->antiAliasing.isHistoryValid();
            vinfo.aaAllocs = vp->antiAliasing.getTargetAllocationCount();
            vinfo.aaDeletes = vp->antiAliasing.getTargetDeletionCount();
            vinfo.targetGeneration = vp->antiAliasing.getTargetGeneration();
            vinfo.lastTaaHistoryValid = vp->antiAliasing.getLastTaaHistoryValid();
            vinfo.lastTaaBlendFactor = vp->antiAliasing.getLastTaaBlendFactor();
            vinfo.lastTaaTargetGeneration = vp->antiAliasing.getLastTaaTargetGeneration();
            vinfo.wireframe = vp->settings.wireframeOverlay;
            vinfo.showGrid = vp->settings.showGrid;
            vinfo.showSceneBoundingBox = vp->settings.showSceneBoundingBox;
            vinfo.forceDoubleSided = vp->settings.forceDoubleSided;
            vinfo.invertWinding = vp->settings.invertWinding;
            vinfo.activePasses = vp->lastExecutedPasses;
            vinfo.renderCount = vp->renderCount;
            uiFrame.viewports.push_back(vinfo);
        }
        uiFrame.setAaMode = [&](const std::string& modeStr, uint32_t vpId) {
            ux3d::imgui::ViewportInstance* targetVp = activeViewport;
            if (vpId != 0) {
                for (const auto& vp : viewports) {
                    if (vp && vp->id == vpId) {
                        targetVp = vp.get();
                        break;
                    }
                }
            }
            if (!targetVp) return false;
            std::string m = toLowerAscii(modeStr);
            if (m == "none" || m == "0")
                targetVp->settings.setLegacyAA(0);
            else if (m == "msaa2" || m == "1")
                targetVp->settings.setLegacyAA(1);
            else if (m == "msaa4" || m == "2")
                targetVp->settings.setLegacyAA(2);
            else if (m == "msaa8" || m == "3")
                targetVp->settings.setLegacyAA(3);
            else if (m == "fxaa" || m == "4")
                targetVp->settings.setLegacyAA(4);
            else if (m == "taa" || m == "5")
                targetVp->settings.setLegacyAA(5);
            else
                return false;
            return true;
        };
        uiFrame.createViewport = [&]() {
            uint32_t nextId = 1;
            for (const auto& vp : viewports) {
                if (vp && vp->id >= nextId) nextId = vp->id + 1;
            }
            std::string vpName = "Viewport " + std::to_string(nextId);
            auto newVp = std::make_unique<ux3d::imgui::ViewportInstance>(nextId, vpName, gladGL.get());
            if (activeViewport != nullptr) {
                newVp->settings = activeViewport->settings;
                newVp->settings.setLegacyAA(5); // New viewports start with MSAA Off + TAA.
                if (activeScene != nullptr) {
                    newVp->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                }
            }
            newVp->requestFocus = true;
            activeViewport = newVp.get();
            selection.selectViewport(newVp->id);
            viewports.push_back(std::move(newVp));
            return true;
        };
        uiFrame.setAnimationPlaying = [&](bool play) {
            if (play && animationPlayer.getMaxDuration() <= 0.0f) return false;
            animationPlayer.setPlaying(play);
            return true;
        };
        uiFrame.focusViewport = [&](uint32_t id) {
            for (const auto& vp : viewports) {
                if (vp && vp->id == id && vp->isOpen) {
                    vp->requestFocus = true;
                    return true;
                }
            }
            return false;
        };
        uiFrame.splitViewport = [&](uint32_t vpId) {
            ImGuiWindow* first = ImGui::FindWindowByName("###Viewport_1");
            if (vpId <= 1 || first == nullptr || first->DockId == 0) return false;
            bool exists = false;
            for (const auto& vp : viewports)
                exists = exists || (vp && vp->id == vpId && vp->isOpen);
            if (!exists) return false;
            ImGuiID left = 0;
            ImGuiID right = 0;
            ImGui::DockBuilderSplitNode(first->DockId, ImGuiDir_Right, 0.5f, &right, &left);
            ImGui::DockBuilderDockWindow("###Viewport_1", left);
            ImGui::DockBuilderDockWindow(("###Viewport_" + std::to_string(vpId)).c_str(), right);
            ImGui::DockBuilderFinish(first->DockId);
            return true;
        };
        uiFrame.toggleViewport = [&](uint32_t vpId) {
            for (const auto& vp : viewports) {
                if (vp && vp->id == vpId) {
                    vp->isOpen = !vp->isOpen;
                    if (vp->isOpen) vp->requestFocus = true;
                    return true;
                }
            }
            return false;
        };
        uiFrame.setViewportSetting = [&](uint32_t vpId, const std::string& key, bool val) {
            ux3d::imgui::ViewportInstance* targetVp = activeViewport;
            if (vpId != 0) {
                for (const auto& vp : viewports) {
                    if (vp && vp->id == vpId) {
                        targetVp = vp.get();
                        break;
                    }
                }
            }
            if (!targetVp) return false;
            std::string k = toLowerAscii(key);
            if (k == "wireframe" || k == "wireframeoverlay")
                targetVp->settings.wireframeOverlay = val;
            else if (k == "grid" || k == "showgrid")
                targetVp->settings.showGrid = val;
            else if (k == "bbox" || k == "boundingbox" || k == "showsceneboundingbox")
                targetVp->settings.showSceneBoundingBox = val;
            else if (k == "doublesided" || k == "forcedoublesided")
                targetVp->settings.forceDoubleSided = val;
            else if (k == "invertwinding")
                targetVp->settings.invertWinding = val;
            else if (k == "invisible" || k == "showinvisiblematerials")
                targetVp->settings.asobo.setApplied("ASOBO_material_invisible", !val);
            else if (k == "asobo")
                targetVp->settings.asobo.enabled = val;
            else
                return false;
            return true;
        };
        uiFrame.setAsoboParam = [&](const std::string& matName, const std::string& extName,
                                    const std::string& paramName, const std::string& value) {
            return ux3d::imgui::ExtensionInspector::instance().setMaterialAsoboParameter(
                gltfModel, matName, extName, paramName, value, sceneRevisions);
        };
        uiFrame.frameNode = [&](const std::string& spec, const std::string& viewStr, float margin) -> nlohmann::json {
            auto* node = findNode(spec);
            if (!node || !activeViewport) return nullptr;
            selection.selectNode(node);
            ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
            sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
            if (!viewStr.empty()) {
                auto ori = parseCameraOrientation(viewStr);
                if (ori.has_value()) {
                    activeViewport->cameraActor.orientUserCamera(ori.value());
                }
            }
            focusCameraOnNode(node, gltfModel, gltfLayer.get(), activeViewport->cameraActor,
                              activeViewport->antiAliasing, sceneRevisions, margin);
            return getCameraJson(activeViewport);
        };
        uiFrame.nodeCandidates = [&](const std::string& spec) -> std::string {
            std::vector<std::string> candidates;
            findNode(spec, &candidates);
            if (candidates.empty()) return {};
            std::string text = " (candidates: ";
            for (size_t i = 0; i < candidates.size(); ++i) text += (i ? ", " : "") + candidates[i];
            return text + ")";
        };
        uiFrame.selectNode = [&](const std::string& spec) -> bool {
            auto* node = findNode(spec);
            if (!node) return false;
            selection.selectNode(node);
            ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
            sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
            return true;
        };
        uiFrame.isolateNode = [&](const std::string& spec, bool children) -> bool {
            auto* node = findNode(spec);
            if (!node || !gltfLayer) return false;
            gltfLayer->setIsolation(node, children);
            selection.selectNode(node);
            ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
            sceneRevisions.mark(ux3d::imgui::SceneChange::Visibility);
            return true;
        };
        uiFrame.exitIsolate = [&]() -> bool {
            if (!gltfLayer) return false;
            gltfLayer->setIsolation(nullptr, false);
            sceneRevisions.mark(ux3d::imgui::SceneChange::Visibility);
            return true;
        };
        uiFrame.setView = [&](const std::string& preset) -> bool {
            if (!activeViewport) return false;
            auto ori = parseCameraOrientation(preset);
            if (!ori.has_value()) return false;
            activeViewport->cameraActor.orientUserCamera(ori.value());
            activeViewport->antiAliasing.resetHistory();
            sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
            return true;
        };
        uiFrame.setCamera = [&](const glm::vec3& eye, const glm::vec3& target, const std::optional<float>& fov,
                                const std::optional<float>& nearPlane, const std::optional<float>& farPlane) -> bool {
            if (!activeViewport) return false;
            activeViewport->cameraActor.setCameraEyeAndTarget(eye, target);
            auto* cam = activeViewport->cameraActor.getUserCamera();
            if (cam != nullptr) {
                float fovY = (fov.has_value() && fov.value() > 0.1f) ? glm::radians(fov.value()) : cam->getYFoV();
                float zn = nearPlane.has_value() ? nearPlane.value() : cam->getZNearPlane();
                float zf = farPlane.has_value() ? farPlane.value() : cam->getZFarPlane();
                if (fov.has_value() || nearPlane.has_value() || farPlane.has_value()) {
                    cam->setPerspective(cam->getAspectRatio(), fovY, zn, zf);
                }
            }
            activeViewport->antiAliasing.resetHistory();
            sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
            return true;
        };
        uiFrame.captureViewport = [&](const std::string& path) -> bool {
            if (!activeViewport) return false;
            int vpW = activeViewport->sceneFbo.getWidth();
            int vpH = activeViewport->sceneFbo.getHeight();
            GLuint tex = activeViewport->cachedDisplayTexture;
            if (tex == 0) tex = activeViewport->overlayFbo.getColorTexture();
            if (tex == 0) tex = activeViewport->sceneFbo.getColorTexture();
            ux3d::imgui::profiling::CaptureRecord rec;
            return captureViewportTexture(tex, vpW, vpH, path, "viewport-texture", frameCount, false, 0.0, rec);
        };
        uiFrame.getNodeRect = [&](const std::string& spec) -> nlohmann::json {
            auto* node = findNode(spec);
            if (!node || !activeViewport) return nullptr;
            return computeNodeScreenRect(node, activeViewport);
        };
        uiFrame.pickPixel = [&](int x, int y) -> nlohmann::json {
            if (!activeViewport) return nullptr;
            return performPickAtPixel(x, y, activeViewport);
        };
        uiFrame.setDebugView = [&](const std::string& name) -> bool {
            if (!activeViewport) return false;
            auto mode = getRenderModeFromName(name);
            if (!mode.has_value()) return false;
            activeViewport->settings.renderMode = mode.value();
            activeViewport->settings.renderModeIdx = getIndexFromRenderMode(mode.value());
            activeViewport->antiAliasing.resetHistory();
            sceneRevisions.mark(ux3d::imgui::SceneChange::RenderSettings);
            return true;
        };
        uiFrame.getState = [&]() -> nlohmann::json {
            nlohmann::json st;
            st["model"] = currentFilePath.empty() ? modelToLoad : currentFilePath;
            st["selection"] = describeUiSelection(selection);
            st["isolation"] = (gltfLayer && gltfLayer->getIsolationRoot())
                ? (gltfLayer->getIsolationRoot()->hasName() ? gltfLayer->getIsolationRoot()->getName() : ("Node " + std::to_string(gltfLayer->getIsolationRoot()->m_ID)))
                : "none";
            st["camera"] = getCameraJson(activeViewport);
            st["nodeCount"] = gltfModel.getNodes().size();
            st["materialCount"] = gltfModel.getElements<ux3d::gltf::Material>().size();
            return st;
        };
        uiFrame.getCameraState = [&]() -> nlohmann::json {
            return getCameraJson(activeViewport);
        };
        uiScenario.endFrame(window, uiFrame);
        if (uiScenario.failed()) {
            s_outputFailed = true;
        }
    }
}


nlohmann::json Application::getCameraJson(const ux3d::imgui::ViewportInstance* vp)
{
    nlohmann::json cam;
    if (vp == nullptr) {
        return cam;
    }
    auto* renCam = vp->cameraActor.getUserCamera();
    const auto eye = vp->cameraActor.getEyePosition();
    const auto target = vp->cameraActor.getLookTarget();
    const auto up = renCam != nullptr ? renCam->getUp() : ux3d::math::vec3(0, 1, 0);
    float fovY = renCam != nullptr ? glm::degrees(renCam->getYFoV()) : 45.0f;
    float zNear = renCam != nullptr ? renCam->getZNearPlane() : 0.1f;
    float zFar = renCam != nullptr ? renCam->getZFarPlane() : 100.0f;
    std::string projection = (renCam != nullptr && renCam->getType() == ux3d::renderer::Camera::Type::Orthographic) ? "orthographic" : "perspective";
    int vpW = vp->sceneFbo.getWidth();
    int vpH = vp->sceneFbo.getHeight();
    float dist = vp->cameraActor.getZoom();
    float yawDeg = glm::degrees(vp->cameraActor.getRotationX());
    float pitchDeg = glm::degrees(vp->cameraActor.getRotationY());

    char cliArgsBuf[512];
    std::snprintf(cliArgsBuf, sizeof(cliArgsBuf),
        "--camera-target %.6f,%.6f,%.6f --camera-distance %.6f --camera-yaw %.6f --camera-pitch %.6f --camera-fov %.6f --camera-projection %s",
        target.x, target.y, target.z, dist, yawDeg, pitchDeg, fovY, projection.c_str());

    cam["eye"] = { eye.x, eye.y, eye.z };
    cam["target"] = { target.x, target.y, target.z };
    cam["up"] = { up.x, up.y, up.z };
    cam["fovY"] = fovY;
    cam["near"] = zNear;
    cam["far"] = zFar;
    cam["projection"] = projection;
    cam["viewport"] = { vpW, vpH };
    cam["cliArgs"] = cliArgsBuf;
    return cam;
}

nlohmann::json Application::computeNodeScreenRect(const ux3d::gltf::Node* node, const ux3d::imgui::ViewportInstance* vp)
{
    nlohmann::json result;
    if (node == nullptr || vp == nullptr) {
        return result;
    }

    const NodeOBB& obb = getCachedNodeOBB(node, gltfLayer.get());
    int vpW = vp->sceneFbo.getWidth();
    int vpH = vp->sceneFbo.getHeight();
    if (vpW < 1) vpW = 1;
    if (vpH < 1) vpH = 1;

    glm::mat4 view = vp->rendererCamera.getViewMatrix();
    glm::mat4 proj = vp->rendererCamera.getProjectionMatrix();
    glm::mat4 viewProj = proj * view;

    glm::vec3 worldCenter;
    if (obb.valid) {
        glm::vec3 localCenter = (obb.localMin + obb.localMax) * 0.5f;
        worldCenter = glm::vec3(obb.modelMatrix * glm::vec4(localCenter, 1.0f));
    } else {
        glm::mat4 nodeWorld = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
        worldCenter = glm::vec3(nodeWorld[3]);
    }

    result["index"] = node->m_ID;
    result["name"] = node->hasName() ? node->getName() : ("Node " + std::to_string(node->m_ID));
    result["visible"] = node->getVisible();
    result["center"] = { worldCenter.x, worldCenter.y, worldCenter.z };

    if (!obb.valid) {
        glm::vec4 clip = viewProj * glm::vec4(worldCenter, 1.0f);
        bool behindCamera = clip.w <= 0.001f;
        float x = 0.0f, y = 0.0f;
        bool onScreen = false;
        if (!behindCamera) {
            float ndcX = clip.x / clip.w;
            float ndcY = clip.y / clip.w;
            x = (ndcX * 0.5f + 0.5f) * static_cast<float>(vpW);
            y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(vpH);
            onScreen = (x >= 0.0f && x <= static_cast<float>(vpW) && y >= 0.0f && y <= static_cast<float>(vpH));
        }
        result["screenRect"] = { x, y, x, y };
        result["onScreen"] = onScreen;
        result["clipped"] = !onScreen;
        result["behindCamera"] = behindCamera;
        return result;
    }

    glm::vec3 corners[8] = {
        { obb.localMin.x, obb.localMin.y, obb.localMin.z },
        { obb.localMax.x, obb.localMin.y, obb.localMin.z },
        { obb.localMax.x, obb.localMin.y, obb.localMax.z },
        { obb.localMin.x, obb.localMin.y, obb.localMax.z },
        { obb.localMin.x, obb.localMax.y, obb.localMin.z },
        { obb.localMax.x, obb.localMax.y, obb.localMin.z },
        { obb.localMax.x, obb.localMax.y, obb.localMax.z },
        { obb.localMin.x, obb.localMax.y, obb.localMax.z }
    };

    glm::vec4 worldCorners[8];
    glm::vec4 clipCorners[8];
    int behindCount = 0;
    for (int i = 0; i < 8; ++i) {
        worldCorners[i] = obb.modelMatrix * glm::vec4(corners[i], 1.0f);
        clipCorners[i] = viewProj * worldCorners[i];
        if (clipCorners[i].w <= 0.001f) {
            ++behindCount;
        }
    }

    bool allBehind = (behindCount == 8);
    result["behindCamera"] = allBehind;

    if (allBehind) {
        result["screenRect"] = { 0.0f, 0.0f, 0.0f, 0.0f };
        result["onScreen"] = false;
        result["clipped"] = true;
        return result;
    }

    static const int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };

    std::vector<glm::vec2> projectedPoints;
    bool anyClipped = (behindCount > 0);

    for (int i = 0; i < 8; ++i) {
        if (clipCorners[i].w > 0.001f) {
            float ndcX = clipCorners[i].x / clipCorners[i].w;
            float ndcY = clipCorners[i].y / clipCorners[i].w;
            float px = (ndcX * 0.5f + 0.5f) * static_cast<float>(vpW);
            float py = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(vpH);
            projectedPoints.push_back({ px, py });
            if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) {
                anyClipped = true;
            }
        }
    }

    constexpr float nearW = 0.001f;
    for (int e = 0; e < 12; ++e) {
        const auto& c0 = clipCorners[edges[e][0]];
        const auto& c1 = clipCorners[edges[e][1]];
        if ((c0.w > nearW && c1.w <= nearW) || (c0.w <= nearW && c1.w > nearW)) {
            float t = (nearW - c0.w) / (c1.w - c0.w);
            glm::vec4 clipped = c0 + t * (c1 - c0);
            float ndcX = clipped.x / nearW;
            float ndcY = clipped.y / nearW;
            float px = (ndcX * 0.5f + 0.5f) * static_cast<float>(vpW);
            float py = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(vpH);
            projectedPoints.push_back({ px, py });
            anyClipped = true;
        }
    }

    if (projectedPoints.empty()) {
        result["screenRect"] = { 0.0f, 0.0f, 0.0f, 0.0f };
        result["onScreen"] = false;
        result["clipped"] = true;
        return result;
    }

    float minX = projectedPoints[0].x, maxX = projectedPoints[0].x;
    float minY = projectedPoints[0].y, maxY = projectedPoints[0].y;
    for (size_t i = 1; i < projectedPoints.size(); ++i) {
        minX = std::min(minX, projectedPoints[i].x);
        maxX = std::max(maxX, projectedPoints[i].x);
        minY = std::min(minY, projectedPoints[i].y);
        maxY = std::max(maxY, projectedPoints[i].y);
    }

    bool onScreen = (maxX >= 0.0f && minX <= static_cast<float>(vpW) &&
                     maxY >= 0.0f && minY <= static_cast<float>(vpH));

    if (minX < 0.0f || maxX > static_cast<float>(vpW) ||
        minY < 0.0f || maxY > static_cast<float>(vpH)) {
        anyClipped = true;
    }

    result["screenRect"] = { minX, minY, maxX, maxY };
    result["onScreen"] = onScreen;
    result["clipped"] = anyClipped;
    return result;
}

nlohmann::json Application::performPickAtPixel(int px, int py, ux3d::imgui::ViewportInstance* vp)
{
    if (vp == nullptr || gltfLayer == nullptr || activeScene == nullptr) {
        return nullptr;
    }
    int vpW = vp->sceneFbo.getWidth();
    int vpH = vp->sceneFbo.getHeight();
    if (vpW < 1 || vpH < 1) {
        return nullptr;
    }

    if (px < 0) px = 0;
    if (px >= vpW) px = vpW - 1;
    if (py < 0) py = 0;
    if (py >= vpH) py = vpH - 1;

    int glPy = vpH - py - 1;

    if (!pickingTarget.isValid() || pickingTarget.getWidth() != 1 || pickingTarget.getHeight() != 1) {
        pickingTarget.resize(1, 1, 1);
    }

    auto* pickCamera = gltfLayer->getActiveCamera();
    if (pickCamera == nullptr) {
        return nullptr;
    }

    const auto sceneProj = pickCamera->getProjectionMatrix();

    auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
    currentReverseMapping = std::move(reverseMapping);

    glm::mat4 pixelWindow(1.0f);
    pixelWindow[0][0] = static_cast<float>(vpW);
    pixelWindow[1][1] = static_cast<float>(vpH);
    pixelWindow[3][0] = static_cast<float>(vpW - 2 * px - 1);
    pixelWindow[3][1] = static_cast<float>(vpH - 2 * glPy - 1);
    pickCamera->setProjection(ux3d::math::mat4x4(pixelWindow * glm::mat4(sceneProj)));

    if (!s_disablePickCulling) {
        gltfLayer->cullToClip(scene.drawables, currentReverseMapping,
                              pickCamera->getProjectionMatrix() * pickCamera->getViewMatrix());
    }

    pickingTarget.bind();
    glViewport(0, 0, 1, 1);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    pickingLayer->prePass = [&]() {
        pickingTarget.bind();
        glViewport(0, 0, 1, 1);
    };
    pickingLayer->postPass = [&]() {
        pickingTarget.bind();
        glViewport(0, 0, 1, 1);
    };

    pickingLayer->pickingOnly = true;
    auto pickingScene = pickingLayer->applyLayer(std::move(scene));
    pickingLayer->pickingOnly = false;

    commandGenerator->update(pickingScene);
    glRenderer->draw({ commandGenerator.get() });
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    pickCamera->setProjection(sceneProj);

    uint8_t pixel[4] = {0, 0, 0, 0};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, pickingTarget.getFbo());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    uint32_t pickingId = (static_cast<uint32_t>(pixel[0]) << 16) |
                         (static_cast<uint32_t>(pixel[1]) << 8) |
                         static_cast<uint32_t>(pixel[2]);

    if (pickingId == 0) {
        return nullptr;
    }

    auto* pickedMesh = pickingLayer->getMesh(pickingId);
    if (pickedMesh == nullptr) {
        return nullptr;
    }

    const ux3d::gltf::Node* pickedNode = nullptr;
    auto nodeIt = currentReverseMapping.find(pickedMesh);
    if (nodeIt != currentReverseMapping.end()) {
        pickedNode = nodeIt->second;
    }

    const ux3d::gltf::Mesh* gltfMesh = nullptr;
    const ux3d::gltf::Primitive* gltfPrim = nullptr;
    int primIdx = 0;

    for (const auto& [key, rendererMesh] : gltfLayer->m_meshMapping.map) {
        if (rendererMesh == pickedMesh) {
            if (!pickedNode) pickedNode = std::get<0>(key);
            gltfMesh = std::get<1>(key);
            gltfPrim = std::get<2>(key);
            break;
        }
    }

    if (gltfMesh != nullptr && gltfPrim != nullptr) {
        int idx = 0;
        for (const auto& p : gltfMesh->getPrimitives()) {
            if (&p == gltfPrim) {
                primIdx = idx;
                break;
            }
            idx++;
        }
    }

    const ux3d::gltf::Material* gltfMat = gltfPrim ? gltfPrim->getMaterial() : nullptr;

    nlohmann::json pickResult;
    pickResult["x"] = px;
    pickResult["y"] = py;
    if (pickedNode != nullptr) {
        pickResult["nodeIndex"] = pickedNode->m_ID;
        std::string nName = pickedNode->hasName() ? pickedNode->getName() : ("Node " + std::to_string(pickedNode->m_ID));
        pickResult["nodeName"] = nName;
        pickResult["node"] = nName;
    } else {
        pickResult["nodeIndex"] = -1;
        pickResult["nodeName"] = "";
        pickResult["node"] = "";
    }

    if (gltfMesh != nullptr) {
        pickResult["mesh"] = gltfMesh->hasName() ? gltfMesh->getName() : ("Mesh " + std::to_string(gltfMesh->m_ID));
    } else {
        pickResult["mesh"] = "";
    }
    pickResult["primitive"] = primIdx;

    if (gltfMat != nullptr) {
        pickResult["material"] = gltfMat->hasName() ? gltfMat->getName() : ("Material " + std::to_string(gltfMat->m_ID));
    } else {
        pickResult["material"] = "";
    }

    return pickResult;
}

} // namespace ux3d::imgui::application
