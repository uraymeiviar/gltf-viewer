// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Diagnostics: self-check reports, the A2.1 decal prepass check, image capture and hashing helpers, report
// naming / JSON helpers, the GPU report and the UI selection description. finishFrame and the scenario report
// are in DiagnosticsFrame.cpp; camera JSON, node screen rectangles and picking in DiagnosticsPicking.cpp.

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

bool Application::readTexturePixels(GLuint texture, int width, int height, std::vector<uint8_t>& pixels, double& readbackMs)
{
    pixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);
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
    readbackMs = ux3d::imgui::profiling::elapsedMs(readbackStart);

    if (readError != GL_NO_ERROR) {
        std::fprintf(stderr, "Capture failed: glGetTexImage error 0x%X\n", readError);
        return false;
    }
    return true;
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

    std::vector<uint8_t> pixels;
    if (!readTexturePixels(texture, width, height, pixels, record.readbackMs)) {
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

} // namespace ux3d::imgui::application
