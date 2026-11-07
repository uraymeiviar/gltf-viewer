// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Image Viewer (header/imgui/ImageViewer.h). Pixel source: the image's encoded bytes are decoded again at full
// size (core::loading::fullResolutionDecode bypasses the texture limit of the load), so the viewer never
// keeps the model's pixels alive and never shows a reduced copy. Block-compressed KTX2 / DDS files decode
// their top level to the channels the format stores (imageformats::StoredPixels); BC5 normal maps also get
// the decoded view the Inspector preview shows (X/Y rebuilt into Z, the packaged signed-square decode), and
// Raw switches to the stored values. The image area is drawn by a small GLSL program installed through an
// ImDrawList callback: channel mask, grayscale, levels and the screen-space checkerboard.

#include "imgui/ImageViewer.h"
#include "ElementInspectorImages.h"
#include "core/LoadProgress.h"
#include "core/Logger.h"
#include "imageformats/Dds.h"
#include "imgui/MeshAttributes.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/SettingsFile.h"
#include "gltf/GltfUtils.h"
#include "gltf/Material.h"
#include "gltf/Mesh.h"
#include "gltf/TextureInfo.h"
#include "gltf/mesh/MaterialVariantsMapping.h"
#include "gltf/mesh/Primitive.h"
#include "renderer/Mesh.h"

#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <map>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <unordered_map>


namespace ux3d::imgui {

namespace {

constexpr int kBins = 256;
constexpr size_t kUploadBytesPerFrame = 24u << 20; // level-0 row bands per frame (all viewers together)
size_t s_uploadBudget = kUploadBytesPerFrame;
constexpr size_t kCopyBytesPerFrame = 16u << 20;

#ifdef _WIN32
// Below-normal priority for the load workers (kernel32; HANDLE = void*, BOOL = int), without <windows.h>'s macros.
extern "C" __declspec(dllimport) void* __stdcall GetCurrentThread(void);
extern "C" __declspec(dllimport) int __stdcall SetThreadPriority(void* thread, int priority);
#endif

using Clock = std::chrono::steady_clock;
double msBetween(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// A background task: one std::thread, an atomic cancel flag checked between phases and in long loops, and a
// finished flag set as the thread's last action. The main thread always holds the last reference and joins the
// thread before dropping it: a finished task is joined at once (it has nothing left to do), a cancelled one waits
// in s_retired until it finishes (the decode itself cannot be interrupted; its result is discarded) and is joined
// there, so closing a window never blocks on a decode. ImageViewer::shutdown joins every task.
struct Task {
    std::atomic<bool> cancel{ false };
    std::atomic<bool> finished{ false };
    std::thread thread;
    virtual ~Task() = default;
};
std::vector<std::shared_ptr<Task>> s_retired;

void lowerThreadPriority()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), -1); // THREAD_PRIORITY_BELOW_NORMAL
#endif
}

// Cancels the task and parks it until its thread ends; the pointer is reset.
template <typename T>
void retireTask(std::shared_ptr<T>& task)
{
    if (!task) return;
    task->cancel = true;
    if (!task->thread.joinable()) {
        task.reset(); // never started (the encoded bytes were still being copied)
        return;
    }
    if (task->finished.load()) {
        task->thread.join();
    } else {
        s_retired.push_back(task);
    }
    task.reset();
}

// Joins a finished task and drops it (call only when finished is set).
template <typename T>
void joinTask(std::shared_ptr<T>& task)
{
    if (!task) return;
    if (task->thread.joinable()) task->thread.join();
    task.reset();
}

void reapRetired(bool wait)
{
    for (auto it = s_retired.begin(); it != s_retired.end();) {
        if (wait || (*it)->finished.load()) {
            if ((*it)->thread.joinable()) (*it)->thread.join();
            it = s_retired.erase(it);
        } else {
            ++it;
        }
    }
}
constexpr float kMinZoom = 0.01f;
constexpr float kMaxZoom = 32.0f;
constexpr float kWheelStep = 1.05f;

enum class SampleType { U8, S8, U16, F32 };

// One interpretation of the image (stored values, or the decoded normal map) with its histogram and texture.
struct PixelSet {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    SampleType type = SampleType::U8;
    bool luminance = false; // channel 0 is L (PNG/JPEG gray): [L] / [L][A]
    uint32_t storedBits = 8; // per channel in the file (16 for half floats shown as 32-bit floats)
    // width * height * channels samples, row 0 at the top. Shared read-only: the load worker computes the histogram
    // from the same pixels the main thread uploads and reads.
    std::shared_ptr<const std::vector<uint8_t>> bytes;
    // What the GPU gets when it differs from bytes (three channels padded to four, signed bytes offset by +128);
    // made by the worker, dropped once uploaded.
    std::shared_ptr<const std::vector<uint8_t>> uploadBytes;
    std::array<std::array<uint32_t, kBins>, 4> histogram{};
    std::array<double, 4> dataMin{};
    std::array<double, 4> dataMax{};
    GLuint texture = 0;
    bool uploadTried = false;
    uint32_t uploadedRows = 0;     // rows of level 0 sent so far (row bands across frames)
    bool textureComplete = false;  // every row sent and the mipmaps generated
    std::string uploadError;

    bool valid() const { return width > 0 && height > 0 && channels > 0 && bytes != nullptr && !bytes->empty(); }
    size_t sampleBytes() const
    {
        switch (type) {
        case SampleType::U16: return 2;
        case SampleType::F32: return 4;
        default: return 1;
        }
    }
    bool integer() const { return type != SampleType::F32; }
    double rangeMin() const { return type == SampleType::S8 ? -128.0 : 0.0; }
    double rangeMax() const
    {
        switch (type) {
        case SampleType::U8: return 255.0;
        case SampleType::S8: return 127.0;
        case SampleType::U16: return 65535.0;
        default: return 1.0;
        }
    }
    double step() const { return integer() ? 1.0 : 1.0 / 1024.0; }
    int alphaIndex() const
    {
        if (luminance && channels == 2) return 1;
        if (!luminance && channels == 4) return 3;
        return -1;
    }
    char label(uint32_t c) const
    {
        if (luminance) return c == 0 ? 'L' : 'A';
        static const char rgba[4] = { 'R', 'G', 'B', 'A' };
        return c < 4 ? rgba[c] : '?';
    }
    double value(uint32_t x, uint32_t y, uint32_t c) const
    {
        const size_t index = (static_cast<size_t>(y) * width + x) * channels + c;
        switch (type) {
        case SampleType::U8: return (*bytes)[index];
        case SampleType::S8: return static_cast<int8_t>((*bytes)[index]);
        case SampleType::U16: {
            uint16_t v = 0;
            std::memcpy(&v, bytes->data() + index * 2, 2);
            return v;
        }
        default: {
            float v = 0.0f;
            std::memcpy(&v, bytes->data() + index * 4, 4);
            return v;
        }
        }
    }
    // 0..1 for swatches: the full native range (floats clamped to [0, 1]).
    double normalized(double v) const
    {
        if (!std::isfinite(v)) return 0.0;
        return std::clamp((v - rangeMin()) / (rangeMax() - rangeMin()), 0.0, 1.0);
    }
    const char* typeName() const
    {
        switch (type) {
        case SampleType::U8: return "uint8";
        case SampleType::S8: return "int8";
        case SampleType::U16: return "uint16";
        default: return "float32";
        }
    }
};

// The UV layout overlay of one window: the triangles of every primitive with the material in the slot's TEXCOORD
// set, gathered once in raw UV space; the edge list (and the faces for Fill) is built from them for the current
// Transform / Wrap choice, deduplicated, and uploaded once into GL buffers drawn in UV space by a small shader (the
// view transform is a uniform, so pan and zoom rebuild nothing).
// Timings of one viewer load (logged once ready, reported in the script state). Worker phases run off the UI
// thread; copyMs, uploadMs and mipmapMs are main-thread time.
struct LoadTimes {
    double copyMs = 0.0;      // main: header + encoded bytes copied for the worker
    double decodeMs = 0.0;    // worker: full-resolution decode (PNG / JPEG / KTX2 BC / DDS)
    double convertMs = 0.0;   // worker: native-format copy + upload layout (padding, signed offset)
    double variantMs = 0.0;   // worker: the decoded normal-map view
    double histogramMs = 0.0; // worker: histogram and data range (one pass per view)
    double uploadMs = 0.0;    // main: glTexSubImage2D row bands, summed
    int uploadFrames = 0;
    double mipmapMs = 0.0;    // main: glGenerateMipmap call (the GPU work runs asynchronously)
    double sharpMs = -1.0;    // since the click: the full-resolution texture replaces the preview
    double readyMs = -1.0;    // since the click: pixels, texture, histogram (and UV overlay) ready
    double maxViewerMs = 0.0; // the viewer's longest main-thread time in one frame during the load
    double maxFrameMs = 0.0;  // the longest frame (ImGui delta time) during the load
    double allocMs = 0.0;     // main: glGenTextures + texture storage
};

// One background load: inputs copied on the main thread; the worker never touches gltf::Image or the model.
struct LoadJob : Task {
    // The encoded bytes are copied on the main thread in chunks of kCopyBytesPerFrame (one per frame, so a 90 MB
    // PNG does not stall the click frame); the worker starts when the copy is complete.
    std::vector<uint8_t> encodedCopy;
    size_t encodedTotal = 0;
    bool copying = false;
    core::Vector<uint8_t> encoded; // made by the worker from encodedCopy (the decoders take core::Vector)
    bool useModelImage = false;  // no encoded bytes: a copy of the model's decoded pixels
    ux3d::image::Image modelImage;
    bool modelLuminance = false;
    std::string modelSource;
    bool signedSquare = false;
    // Results, published by stage (release / acquire): 1 = pixels (or an error), 2 = histograms.
    std::atomic<int> stage{ 0 };
    PixelSet stored;
    PixelSet decoded;
    bool hasDecoded = false;
    std::string source;
    std::string decodedNote;
    std::string error;
    std::vector<std::string> warnings;
    double decodeMs = 0.0;
    double convertMs = 0.0;
    double variantMs = 0.0;
    double histogramMs = 0.0;
};

constexpr size_t kMaxUvEdges = 5000000;

// One primitive's UVs (decoded floats) and triangles (vertex indices), read on the main thread from the model;
// the edge builds on the worker only read these copies.
struct UvSource {
    std::shared_ptr<const std::vector<float>> uvs;
    std::shared_ptr<const std::vector<std::array<uint32_t, 3>>> triangles;
};

// One edge build on a worker thread: the edges (and faces for Fill) for one Transform / Wrap / Fill choice.
struct UvBuildJob : Task {
    std::vector<UvSource> sources;
    std::array<double, 9> transform{};
    bool applyTransform = false; // the slot has a transform and Transform is on
    bool transformOption = true;
    bool wrap = false;
    bool fill = false;
    std::vector<float> lines;
    std::vector<float> faces;
    uint64_t outside = 0;
    bool capped = false;
    double buildMs = 0.0;
};

struct UvOverlay {
    const ux3d::gltf::Material* material = nullptr;
    uint32_t materialIndex = 0;
    std::string materialName;
    std::string slot;
    uint32_t texCoord = 0;
    bool hasTransform = false;
    std::array<double, 9> transform{ 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

    std::vector<UvSource> sources;
    size_t triangleCount = 0;
    uint32_t primitives = 0;           // triangle primitives drawn with the material and the UV set
    uint32_t skippedPrimitives = 0;    // points / lines with the material
    uint32_t missingUvPrimitives = 0;  // triangle primitives without the TEXCOORD set
    uint32_t unreadablePrimitives = 0; // unreadable UVs or indices
    double gatherMs = 0.0;

    bool show = true;
    bool wrap = false;
    bool fill = false;
    bool applyTransform = true;
    float opacity = 0.6f;

    // The buffers drawn now (the last finished build); a new build replaces them only when it is done.
    bool built = false;
    bool builtWrap = false;
    bool builtTransform = true;
    bool builtFill = false;
    uint64_t outside = 0; // triangles partly outside 0..1 (as shown: after the transform when it is applied)
    size_t edges = 0;
    size_t fillTriangles = 0;
    bool capped = false;
    double buildMs = 0.0;       // worker
    double bufferUploadMs = 0.0; // main: glBufferData of the finished build
    double buildMaxViewerMs = 0.0; // the viewer's longest main-thread frame while the last build ran
    double buildMaxFrameMs = 0.0;  // the longest frame while the last build ran
    GLuint lineBuffer = 0;
    GLuint fillBuffer = 0;
    std::shared_ptr<UvBuildJob> job; // the build in flight
};

struct ViewerWindow {
    std::string key;
    std::string title;
    std::string displayName;
    const ux3d::gltf::Image* image = nullptr;
    uint32_t imageIndex = 0;
    int encodedMapID = -1;
    bool signedSquare = false;
    bool needsReload = false;

    PixelSet stored;
    PixelSet decoded;
    bool hasDecoded = false;
    std::string source;      // "PNG", "KTX2 BC7_UNORM, top level", ...
    std::string decodedNote; // what the decoded view rebuilds
    std::vector<std::string> warnings;
    std::string error;       // nothing can be shown

    bool raw = false;
    bool fit = true;
    float zoom = 1.0f;
    double centerX = 0.0; // image coordinate shown at the centre of the area
    double centerY = 0.0;
    std::array<bool, 4> on{ true, true, true, true };
    int background = 0; // checkerboard, white, gray, black
    double low = 0.0;
    double high = 255.0;
    bool haveReadout = false;
    uint32_t readX = 0;
    uint32_t readY = 0;
    int dragLine = -1;
    bool focusRequest = true;
    ImVec2 areaMin{ 0.0f, 0.0f };
    ImVec2 areaSize{ 0.0f, 0.0f };
    ImVec2 imageMin{ 0.0f, 0.0f }; // the image's screen rectangle of the last frame
    ImVec2 imageMax{ 0.0f, 0.0f };
    std::unique_ptr<UvOverlay> uv;
    ImVec2 windowMin{ 0.0f, 0.0f };
    ImVec2 windowSize{ 0.0f, 0.0f };
    bool detached = false; // in its own OS window (multi-viewport)
    bool pendingPos = false;
    ImVec2 requestedPos{ 0.0f, 0.0f };
    bool pendingSize = false;
    ImVec2 requestedSize{ 0.0f, 0.0f };

    // Crosshair: the hovered image pixel this frame, else a pixel a script pinned (image_viewer cursor=x,y).
    bool crossHover = false;
    uint32_t crossX = 0;
    uint32_t crossY = 0;
    bool crossPinned = false;
    uint32_t crossPinX = 0;
    uint32_t crossPinY = 0;
    bool crossDrawn = false;        // drawn in the last frame
    ImVec2 crossScreen{ 0.0f, 0.0f }; // its screen pixel column / row
    int gridLines[2] = { 0, 0 };    // major / minor lines drawn in the last frame

    // Background load (decode, histogram) and the GL texture made in row bands; until the texture is complete the
    // window shows previewTexture (the Inspector's preview or the renderer's texture of the image), if any.
    std::shared_ptr<LoadJob> job;
    bool pixelsReady = false; // stored / decoded pixels (or error) adopted from the worker: readout available
    bool statsReady = false;  // histograms and data ranges adopted
    bool readyLogged = false;
    GLuint previewTexture = 0;
    Clock::time_point loadStart{};
    LoadTimes times;
    nlohmann::json pendingOptions; // image_viewer view options sent while the pixels were loading

    PixelSet& active() { return hasDecoded && !raw ? decoded : stored; }
    const PixelSet& active() const { return hasDecoded && !raw ? decoded : stored; }
};

std::map<std::string, std::unique_ptr<ViewerWindow>> s_windows;

GLuint s_program = 0;
bool s_programFailed = false;
GLint s_locProj = -1, s_locTexture = -1, s_locChannelCount = -1, s_locAlphaIndex = -1, s_locLuminance = -1,
      s_locOn = -1, s_locNative = -1, s_locLevels = -1, s_locBackground = -1, s_locChecker = -1;
GLuint s_samplerNearest = 0;
GLuint s_samplerLinear = 0;
GLint s_imguiProgram = 0;
GLint s_imguiProjLoc = -1;

const char* const kBackgroundNames[] = { "Checkerboard", "White", "Gray", "Black" };
const char* const kBackgroundKeys[] = { "checker", "white", "gray", "black" };

// ---------------------------------------------------------------------------------------------
// Pixel decoding
// ---------------------------------------------------------------------------------------------

float halfToFloat(uint16_t h)
{
    const uint32_t sign = (h >> 15) & 1u;
    const uint32_t exponent = (h >> 10) & 0x1Fu;
    const uint32_t mantissa = h & 0x3FFu;
    float v = 0.0f;
    if (exponent == 0u) {
        v = std::ldexp(static_cast<float>(mantissa), -24);
    } else if (exponent == 31u) {
        v = mantissa != 0u ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
    } else {
        v = std::ldexp(static_cast<float>(mantissa | 0x400u), static_cast<int>(exponent) - 25);
    }
    return sign != 0u ? -v : v;
}

// One pass over the pixels: the 256-bin histogram and the data range (min / max) of every channel. False when
// cancelled (checked every 64 rows).
bool computeHistogram(PixelSet& px, const std::atomic<bool>* cancel = nullptr)
{
    const std::vector<uint8_t>& bytes = *px.bytes;
    for (auto& channel : px.histogram) channel.fill(0u);
    for (uint32_t c = 0; c < 4; ++c) {
        px.dataMin[c] = std::numeric_limits<double>::infinity();
        px.dataMax[c] = -std::numeric_limits<double>::infinity();
    }
    const size_t pixelCount = static_cast<size_t>(px.width) * px.height;
    const uint32_t n = px.channels;
    const size_t checkEvery = static_cast<size_t>(px.width) * 64;
    for (size_t i = 0; i < pixelCount; ++i) {
        if (cancel != nullptr && i % checkEvery == 0 && cancel->load(std::memory_order_relaxed)) return false;
        for (uint32_t c = 0; c < n; ++c) {
            const size_t index = i * n + c;
            int bin = 0;
            double v = 0.0;
            switch (px.type) {
            case SampleType::U8:
                v = bytes[index];
                bin = bytes[index];
                break;
            case SampleType::S8:
                v = static_cast<int8_t>(bytes[index]);
                bin = static_cast<int>(v) + 128;
                break;
            case SampleType::U16: {
                uint16_t s = 0;
                std::memcpy(&s, bytes.data() + index * 2, 2);
                v = s;
                bin = s >> 8;
                break;
            }
            default: {
                float f = 0.0f;
                std::memcpy(&f, bytes.data() + index * 4, 4);
                if (std::isnan(f)) continue;
                v = f;
                // Floats: the histogram covers [0, 1]; values outside count in the end bins.
                bin = static_cast<int>(std::floor(std::clamp(f, 0.0f, 1.0f) * static_cast<float>(kBins)));
                break;
            }
            }
            ++px.histogram[c][static_cast<size_t>(std::clamp(bin, 0, kBins - 1))];
            px.dataMin[c] = std::min(px.dataMin[c], v);
            px.dataMax[c] = std::max(px.dataMax[c], v);
        }
    }
    for (uint32_t c = 0; c < n; ++c) {
        if (px.dataMin[c] > px.dataMax[c]) px.dataMin[c] = px.dataMax[c] = 0.0;
    }
    return true;
}

void fromStored(const ux3d::imageformats::StoredPixels& stored, PixelSet& out)
{
    out = PixelSet();
    out.width = stored.width;
    out.height = stored.height;
    out.channels = stored.channels;
    out.type = stored.signedValues ? SampleType::S8 : SampleType::U8;
    out.bytes = std::make_shared<const std::vector<uint8_t>>(stored.pixels.begin(), stored.pixels.end());
}

// RGBA8 (the decoded normal map) -> RGB, alpha dropped (always opaque).
void fromRgba8(const uint8_t* rgba, uint32_t width, uint32_t height, PixelSet& out)
{
    out = PixelSet();
    out.width = width;
    out.height = height;
    out.channels = 3;
    out.type = SampleType::U8;
    auto bytes = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(width) * height * 3);
    for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
        (*bytes)[i * 3 + 0] = rgba[i * 4 + 0];
        (*bytes)[i * 3 + 1] = rgba[i * 4 + 1];
        (*bytes)[i * 3 + 2] = rgba[i * 4 + 2];
    }
    out.bytes = std::move(bytes);
}

// An uncompressed image::Image (level 0, face 0, layer 0) with uniform channel sizes.
bool fromImage(const ux3d::image::Image& data, bool luminanceLabels, PixelSet& out, std::string& error,
               std::vector<std::string>& warnings)
{
    if (data.isCompressed() || data.isPacked()) {
        error = "pixel layout " + data.getMemoryLayoutString() + " (compressed or packed) is not supported";
        return false;
    }
    const uint32_t n = data.getChannelCount();
    if (n < 1 || n > 4) {
        error = "unsupported channel count " + std::to_string(n);
        return false;
    }
    const uint32_t bits = data.getChannelSize(ux3d::image::Channel::RED);
    for (uint32_t c = 1; c < n; ++c) {
        if (data.getChannelSize(static_cast<ux3d::image::Channel>(c)) != bits) {
            error = "mixed channel sizes (" + data.getMemoryLayoutString() + ") are not supported";
            return false;
        }
    }
    const bool isFloat = data.getFormat() == ux3d::image::Format::FLOAT;
    const bool isSigned = data.isSigned();
    SampleType type = SampleType::U8;
    if (isFloat && (bits == 32 || bits == 16)) {
        type = SampleType::F32;
        if (bits == 16) warnings.push_back("16-bit float values are shown as 32-bit floats");
    } else if (!isFloat && bits == 8) {
        type = isSigned ? SampleType::S8 : SampleType::U8;
    } else if (!isFloat && bits == 16 && !isSigned) {
        type = SampleType::U16;
    } else {
        error = "pixel layout " + data.getMemoryLayoutString() + (isSigned ? " signed" : "") + " is not supported";
        return false;
    }
    const uint32_t width = data.getWidth();
    const uint32_t height = data.getHeight();
    const size_t channelBytes = bits / 8;
    const size_t pixelBytes = data.getPixelSize();
    const size_t base = data.getOffset(0, 0, 0, 0, 0, 0);
    const auto& pixels = data.getPixels();
    if (width == 0 || height == 0 || pixelBytes != channelBytes * n
        || pixels.size() < base + static_cast<size_t>(width) * height * pixelBytes) {
        error = "the decoded pixels do not match their layout";
        return false;
    }
    out = PixelSet();
    out.width = width;
    out.height = height;
    out.channels = n;
    out.type = type;
    out.luminance = luminanceLabels && n <= 2;
    out.storedBits = bits;
    auto bytes = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(width) * height * n * out.sampleBytes());
    std::array<size_t, 4> offsets{};
    for (uint32_t c = 0; c < n; ++c) {
        offsets[c] = data.getChannelIndex(static_cast<ux3d::image::Channel>(c)) * channelBytes;
        if (offsets[c] + channelBytes > pixelBytes) {
            error = "the channel order of " + data.getMemoryLayoutString() + " is not supported";
            return false;
        }
    }
    const size_t pixelCount = static_cast<size_t>(width) * height;
    for (size_t i = 0; i < pixelCount; ++i) {
        const uint8_t* src = pixels.data() + base + i * pixelBytes;
        for (uint32_t c = 0; c < n; ++c) {
            const uint8_t* sample = src + offsets[c];
            uint8_t* dst = bytes->data() + (i * n + c) * out.sampleBytes();
            if (isFloat && bits == 16) {
                uint16_t h = 0;
                std::memcpy(&h, sample, 2);
                const float f = halfToFloat(h);
                std::memcpy(dst, &f, 4);
            } else {
                std::memcpy(dst, sample, channelBytes);
            }
        }
    }
    out.bytes = std::move(bytes);
    return true;
}

bool isLuminanceEncoding(ux3d::imageformats::ImageEncoding encoding)
{
    using ux3d::imageformats::ImageEncoding;
    return encoding == ImageEncoding::PNG || encoding == ImageEncoding::JPEG || encoding == ImageEncoding::BMP
        || encoding == ImageEncoding::TGA || encoding == ImageEncoding::WEBP || encoding == ImageEncoding::Undefined;
}

void releaseTextures(ViewerWindow& w)
{
    for (PixelSet* px : { &w.stored, &w.decoded }) {
        if (px->texture != 0) glDeleteTextures(1, &px->texture);
        px->texture = 0;
    }
}

void releaseOverlayBuffers(UvOverlay& o)
{
    // Buffers are shared between the GL contexts of detached windows; any current context can delete them.
    if (o.lineBuffer != 0) glDeleteBuffers(1, &o.lineBuffer);
    if (o.fillBuffer != 0) glDeleteBuffers(1, &o.fillBuffer);
    o.lineBuffer = 0;
    o.fillBuffer = 0;
    o.built = false;
}

void dropOverlay(ViewerWindow& w)
{
    if (w.uv) retireTask(w.uv->job);
    if (w.uv) releaseOverlayBuffers(*w.uv);
    w.uv.reset();
}

// Closing a window: its textures and its UV overlay.
void releaseWindow(ViewerWindow& w)
{
    retireTask(w.job);
    releaseTextures(w);
    dropOverlay(w);
    w.previewTexture = 0;
}

void resetLevels(ViewerWindow& w);
void continueLoad(ViewerWindow& w);

// ---------------------------------------------------------------------------------------------
// Background load: decode on a worker, adopt on the main thread, upload in row bands
// ---------------------------------------------------------------------------------------------

// Worker: the upload layout when it differs from the stored samples (three channels padded to four, because
// RGB8/RGB16/RGB32F are not required to be colour-renderable for glGenerateMipmap; signed bytes offset by +128,
// which the shader subtracts again).
void prepareUpload(PixelSet& px)
{
    if (!px.valid()) return;
    const uint32_t n = px.channels == 3 ? 4u : px.channels;
    if (n == px.channels && px.type != SampleType::S8) return;
    const size_t sampleBytes = px.sampleBytes();
    const size_t pixelCount = static_cast<size_t>(px.width) * px.height;
    auto converted = std::make_shared<std::vector<uint8_t>>(pixelCount * n * sampleBytes);
    const uint8_t* source = px.bytes->data();
    for (size_t i = 0; i < pixelCount; ++i) {
        uint8_t* dst = converted->data() + i * n * sampleBytes;
        std::memcpy(dst, source + i * px.channels * sampleBytes, px.channels * sampleBytes);
        if (px.type == SampleType::S8) {
            for (uint32_t c = 0; c < px.channels; ++c) dst[c] = static_cast<uint8_t>(dst[c] ^ 0x80u);
        }
        if (n != px.channels) {
            uint8_t* alpha = dst + px.channels * sampleBytes;
            if (px.type == SampleType::F32) {
                const float one = 1.0f;
                std::memcpy(alpha, &one, 4);
            } else {
                std::memset(alpha, 0xFF, sampleBytes);
            }
        }
    }
    px.uploadBytes = std::move(converted);
}

// Worker: the decode of the copied bytes into the job (the former synchronous loadPixels).
void decodeJob(LoadJob& j)
{
    namespace fmt = ux3d::imageformats;
    if (j.useModelImage) {
        const auto t0 = Clock::now();
        std::string error;
        if (!fromImage(j.modelImage, j.modelLuminance, j.stored, error, j.warnings)) {
            j.error = "The image cannot be shown: " + error + ".";
            return;
        }
        j.convertMs = msBetween(t0, Clock::now());
        j.source = j.modelSource;
        j.warnings.push_back("decoded copy of the model (no file bytes to decode again)");
        return;
    }
    const core::Vector<uint8_t>& encoded = j.encoded;
    const fmt::ImageEncoding encoding = fmt::getEncodingFromBuffer(encoded);
    fmt::KTX2ImageInfo ktx2;
    const bool isKtx2 = fmt::getKTX2ImageInfo(encoded, ktx2);
    if (isKtx2 && ktx2.blockCompressed) {
        fmt::StoredPixels storedPixels;
        const std::string formatName = fmt::getKTX2FormatName(ktx2.format);
        auto t0 = Clock::now();
        const bool decodedOk = fmt::decodeKTX2StoredPixels(encoded, storedPixels);
        j.decodeMs = msBetween(t0, Clock::now());
        if (!decodedOk) {
            j.error = "KTX2 " + formatName + (ktx2.supercompressionScheme != 0 ? " (supercompressed)" : "")
                + " cannot be decoded for viewing.";
            return;
        }
        if (j.cancel) return;
        t0 = Clock::now();
        fromStored(storedPixels, j.stored);
        j.convertMs = msBetween(t0, Clock::now());
        j.source = "KTX2 " + formatName + ", top level decoded";
        if (ktx2.format == ux3d::gpu::Format::BC5_SNORM_BLOCK && !j.cancel) {
            t0 = Clock::now();
            core::Vector<uint8_t> rgba;
            uint32_t dw = 0;
            uint32_t dh = 0;
            if (fmt::decodeKTX2Preview(encoded, std::max(ktx2.width, ktx2.height), rgba, dw, dh, j.signedSquare)
                && dw == ktx2.width && dh == ktx2.height) {
                fromRgba8(rgba.data(), dw, dh, j.decoded);
                j.hasDecoded = true;
                j.decodedNote = j.signedSquare ? "Decoded normal map: signed-square X/Y, Z rebuilt, shown as n * 0.5 + 0.5"
                                               : "Decoded normal map: Z rebuilt from X/Y, shown as n * 0.5 + 0.5";
            }
            j.variantMs = msBetween(t0, Clock::now());
        }
        return;
    }
    if (encoding == fmt::ImageEncoding::DDS) {
        fmt::StoredPixels storedPixels;
        auto t0 = Clock::now();
        const bool decodedOk = fmt::decodeDDSStoredPixels(encoded, storedPixels);
        j.decodeMs = msBetween(t0, Clock::now());
        if (decodedOk) {
            t0 = Clock::now();
            fromStored(storedPixels, j.stored);
            j.convertMs = msBetween(t0, Clock::now());
            j.source = "DDS, top level decoded";
            if (storedPixels.channels == 2 && !j.cancel) {
                // BC5: decodeDDS rebuilds Z exactly as the renderer's DDS path stores it.
                t0 = Clock::now();
                ux3d::image::Image rgba;
                if (!core::Failed(fmt::decodeDDS(encoded, rgba)) && rgba.getChannelCount() == 4
                    && rgba.getWidth() == storedPixels.width && rgba.getHeight() == storedPixels.height) {
                    fromRgba8(rgba.getPixels().data(), rgba.getWidth(), rgba.getHeight(), j.decoded);
                    j.hasDecoded = true;
                    j.decodedNote = "Decoded normal map: Z rebuilt from X/Y, shown as n * 0.5 + 0.5";
                }
                j.variantMs = msBetween(t0, Clock::now());
            }
            return;
        }
        j.warnings.push_back("BC6H: the DDS decoder clamps the float values to 8 bits");
    }
    ux3d::image::Image decodedImage;
    {
        // fullResolutionDecode is thread-local: set for this worker thread only (runLoad).
        std::string reason;
        const auto t0 = Clock::now();
        const auto result = fmt::decodeImage(encoded, decodedImage, 0, &reason);
        j.decodeMs = msBetween(t0, Clock::now());
        if (core::Failed(result)) {
            j.error = "The image could not be decoded" + (reason.empty() ? std::string(".") : ": " + reason);
            return;
        }
    }
    if (j.cancel) return;
    const auto t0 = Clock::now();
    std::string error;
    if (!fromImage(decodedImage, !isKtx2 && isLuminanceEncoding(encoding), j.stored, error, j.warnings)) {
        j.error = "The image cannot be shown: " + error + ".";
        return;
    }
    j.convertMs = msBetween(t0, Clock::now());
    j.source = isKtx2 ? std::string("KTX2 ") + fmt::getKTX2FormatName(ktx2.format)
                      : std::string(fmt::imageEncodingToString(encoding).c_str());
    if (isKtx2 && ktx2.levelCount > 1) j.source += ", top level";
}

// The worker thread: decode, upload layout, publish the pixels (stage 1), then the histograms (stage 2). Every
// phase checks the cancel flag; a cancelled job publishes nothing more and its results are discarded.
void runLoad(LoadJob& j)
{
    lowerThreadPriority();
    core::loading::fullResolutionDecode = true; // thread-local: bypasses the load's texture limit in this worker
    try {
        if (!j.encodedCopy.empty()) {
            j.encoded.assign(j.encodedCopy.begin(), j.encodedCopy.end());
            std::vector<uint8_t>().swap(j.encodedCopy);
        }
        decodeJob(j);
        if (!j.cancel && j.error.empty()) {
            const auto t0 = Clock::now();
            prepareUpload(j.stored);
            if (j.hasDecoded) prepareUpload(j.decoded);
            j.convertMs += msBetween(t0, Clock::now());
        }
    } catch (const std::bad_alloc&) {
        j.stored = PixelSet();
        j.decoded = PixelSet();
        j.hasDecoded = false;
        j.error = "Not enough memory to decode the image at full size.";
    } catch (...) {
        j.stored = PixelSet();
        j.decoded = PixelSet();
        j.hasDecoded = false;
        j.error = "The image could not be decoded.";
    }
    j.encoded = core::Vector<uint8_t>(); // the copy is not needed any more
    if (!j.cancel) {
        j.stage.store(1, std::memory_order_release);
        if (j.error.empty()) {
            const auto t0 = Clock::now();
            const bool ok = computeHistogram(j.stored, &j.cancel) && (!j.hasDecoded || computeHistogram(j.decoded, &j.cancel));
            j.histogramMs = msBetween(t0, Clock::now());
            if (ok) j.stage.store(2, std::memory_order_release);
        }
    }
    j.finished.store(true, std::memory_order_release);
}

// The size from the file header (no decode), else the model's pixels or its resolution.
void knownSize(const ux3d::gltf::Image* img, uint32_t& width, uint32_t& height)
{
    width = 0;
    height = 0;
    if (img->getEncodedBytes() != nullptr && !img->getEncodedBytes()->empty()) {
        const ImageHeaderDetails details = getImageHeaderDetails(img);
        width = details.width;
        height = details.height;
    }
    if (width == 0 || height == 0) {
        const auto resolution = img->getResolution();
        if (resolution.size() >= 2) {
            width = resolution[0];
            height = resolution[1];
        }
    }
    if (width == 0 || height == 0) {
        width = img->getData().getWidth();
        height = img->getData().getHeight();
    }
}

// Main thread: cancels the window's running load, copies what the worker needs and starts it. The window keeps
// its view; until the pixels arrive it has the header size, no channels, no readout and no histogram.
void startLoad(ViewerWindow& w)
{
    namespace fmt = ux3d::imageformats;
    const auto t0 = Clock::now();
    retireTask(w.job);
    releaseTextures(w);
    w.stored = PixelSet();
    w.decoded = PixelSet();
    w.hasDecoded = false;
    w.source.clear();
    w.decodedNote.clear();
    w.warnings.clear();
    w.error.clear();
    w.pixelsReady = false;
    w.statsReady = false;
    w.readyLogged = false;
    w.haveReadout = false;
    w.times = LoadTimes();
    w.loadStart = t0;
    const ux3d::gltf::Image* img = w.image;
    w.encodedMapID = img->encodedMapID;
    if (img->payloadOmitted()) {
        w.error = "The image was not loaded (the file was opened without textures).";
        w.pixelsReady = true;
        w.statsReady = true;
        return;
    }
    uint32_t width = 0;
    uint32_t height = 0;
    knownSize(img, width, height);
    w.stored.width = std::max(width, 1u);
    w.stored.height = std::max(height, 1u);

    auto job = std::make_shared<LoadJob>();
    job->signedSquare = w.signedSquare;
    const core::Vector<uint8_t>* encoded = img->getEncodedBytes();
    if (encoded != nullptr && !encoded->empty()) {
        job->encodedTotal = encoded->size();
        job->encodedCopy.reserve(job->encodedTotal);
        job->copying = true;
    } else {
        // No encoded bytes to decode again: copy the model's own decoded pixels (kept for such images).
        auto* mutableImg = const_cast<ux3d::gltf::Image*>(img);
        const bool wasReleased = img->pixelsReleased();
        if (wasReleased && !mutableImg->ensureDecoded()) {
            w.error = "The image could not be decoded.";
            w.pixelsReady = true;
            w.statsReady = true;
            return;
        }
        ensureImageDataDecoded(img);
        job->useModelImage = true;
        job->modelImage = img->getData();
        job->modelLuminance = isLuminanceEncoding(img->getMimeType());
        job->modelSource = std::string(fmt::imageEncodingToString(img->getMimeType()).c_str());
        if (wasReleased) mutableImg->releaseDecodedPixels();
        w.stored.width = std::max(job->modelImage.getWidth(), 1u);
        w.stored.height = std::max(job->modelImage.getHeight(), 1u);
    }
    w.times.copyMs = msBetween(t0, Clock::now());
    w.job = std::move(job);
    continueLoad(w);
}

// Main thread, every frame until the worker runs: the next chunk of the encoded bytes (the window's image is
// still in the model and unchanged: render() restarts or closes the window otherwise), then the worker.
void continueLoad(ViewerWindow& w)
{
    if (!w.job || w.job->thread.joinable()) return;
    LoadJob& j = *w.job;
    if (j.copying) {
        const auto t0 = Clock::now();
        const core::Vector<uint8_t>* encoded = w.image != nullptr ? w.image->getEncodedBytes() : nullptr;
        if (encoded == nullptr || encoded->size() != j.encodedTotal) {
            startLoad(w); // the bytes changed under the copy: start again
            return;
        }
        const size_t done = j.encodedCopy.size();
        const size_t chunk = std::min(kCopyBytesPerFrame, j.encodedTotal - done);
        const uint8_t* source = encoded->data() + done;
        j.encodedCopy.insert(j.encodedCopy.end(), source, source + chunk);
        w.times.copyMs += msBetween(t0, Clock::now());
        if (j.encodedCopy.size() < j.encodedTotal) return;
        j.copying = false;
    }
    LoadJob* raw = &j;
    j.thread = std::thread([raw] { runLoad(*raw); }); // the window (or s_retired) keeps the job alive until joined
}

// Copies the published metadata and the shared pixel pointers (never the histograms, which the worker may still
// be writing).
void adoptPixels(PixelSet& dst, const PixelSet& src)
{
    dst = PixelSet();
    dst.width = src.width;
    dst.height = src.height;
    dst.channels = src.channels;
    dst.type = src.type;
    dst.luminance = src.luminance;
    dst.storedBits = src.storedBits;
    dst.bytes = src.bytes;
    dst.uploadBytes = src.uploadBytes;
}

void adoptStats(PixelSet& dst, const PixelSet& src)
{
    dst.histogram = src.histogram;
    dst.dataMin = src.dataMin;
    dst.dataMax = src.dataMax;
}

bool applyViewOptions(ViewerWindow& w, const nlohmann::json& args, std::string& errorCode, std::string& errorMessage);

// Main thread, every frame: adopts what the worker published.
void pollLoad(ViewerWindow& w)
{
    if (!w.job) return;
    LoadJob& j = *w.job;
    const int stage = j.stage.load(std::memory_order_acquire);
    if (stage >= 1 && !w.pixelsReady) {
        const uint32_t previousWidth = w.stored.width;
        const uint32_t previousHeight = w.stored.height;
        w.error = j.error;
        w.source = j.source;
        w.decodedNote = j.decodedNote;
        w.warnings = j.warnings;
        w.hasDecoded = j.hasDecoded;
        w.times.decodeMs = j.decodeMs;
        w.times.convertMs = j.convertMs;
        w.times.variantMs = j.variantMs;
        if (w.error.empty()) {
            adoptPixels(w.stored, j.stored);
            if (j.hasDecoded) adoptPixels(w.decoded, j.decoded);
        }
        w.pixelsReady = true;
        if (!w.error.empty()) {
            w.statsReady = true;
        } else {
            // Channels and levels for the real pixels; the zoom and the position stay (the header size was right).
            w.on = { true, true, true, true };
            resetLevels(w);
            const PixelSet& px = w.active();
            if (px.width != previousWidth || px.height != previousHeight) {
                w.centerX = px.width * 0.5;
                w.centerY = px.height * 0.5;
            }
            if (!w.pendingOptions.is_null()) {
                const nlohmann::json pending = std::move(w.pendingOptions);
                w.pendingOptions = nlohmann::json();
                std::string code;
                std::string message;
                if (!applyViewOptions(w, pending, code, message)) {
                    core::logWarning("[Image Viewer] script options after loading: %s", message.c_str());
                }
            }
        }
    }
    if (stage >= 2 && !w.statsReady) {
        adoptStats(w.stored, j.stored);
        if (w.hasDecoded) adoptStats(w.decoded, j.decoded);
        w.times.histogramMs = j.histogramMs;
        w.statsReady = true;
        if (!w.pendingOptions.is_null()) { // levels data, sent before the data range was known
            const nlohmann::json pending = std::move(w.pendingOptions);
            w.pendingOptions = nlohmann::json();
            std::string code;
            std::string message;
            applyViewOptions(w, pending, code, message);
        }
    }
    if (j.finished.load(std::memory_order_acquire)) {
        if (!w.statsReady) w.statsReady = true; // the histogram pass did not finish (out of memory): none shown
        joinTask(w.job);
    }
}

// Main thread: allocates the texture on the first call, then sends level 0 in row bands of at most `budget`
// bytes (at least one row), and generates the mipmaps once every row is there.
void uploadStep(PixelSet& px, size_t& budget, LoadTimes* times)
{
    if (px.textureComplete || !px.valid() || !px.uploadError.empty()) return;
    const uint32_t n = px.channels == 3 ? 4u : px.channels;
    static const GLenum formats[4] = { GL_RED, GL_RG, GL_RGB, GL_RGBA };
    GLenum internalFormat = GL_RGBA8;
    GLenum type = GL_UNSIGNED_BYTE;
    switch (px.type) {
    case SampleType::U16:
        type = GL_UNSIGNED_SHORT;
        internalFormat = n == 1 ? GL_R16 : n == 2 ? GL_RG16 : GL_RGBA16;
        break;
    case SampleType::F32:
        type = GL_FLOAT;
        internalFormat = n == 1 ? GL_R32F : n == 2 ? GL_RG32F : GL_RGBA32F;
        break;
    default:
        internalFormat = n == 1 ? GL_R8 : n == 2 ? GL_RG8 : GL_RGBA8;
        break;
    }
    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    if (!px.uploadTried) {
        px.uploadTried = true;
        GLint maxSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
        if (static_cast<GLint>(px.width) > maxSize || static_cast<GLint>(px.height) > maxSize) {
            px.uploadError = "larger than the GPU's maximum texture size " + std::to_string(maxSize);
            return;
        }
        const auto a0 = Clock::now();
        glGenTextures(1, &px.texture);
        if (px.texture == 0) {
            px.uploadError = "glGenTextures failed";
            return;
        }
        glBindTexture(GL_TEXTURE_2D, px.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (GLAD_GL_VERSION_4_2) {
            GLsizei levels = 1;
            for (uint32_t size = std::max(px.width, px.height); size > 1; size >>= 1) ++levels;
            glTexStorage2D(GL_TEXTURE_2D, levels, internalFormat, static_cast<GLsizei>(px.width), static_cast<GLsizei>(px.height));
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), static_cast<GLsizei>(px.width),
                         static_cast<GLsizei>(px.height), 0, formats[n - 1], type, nullptr);
        }
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
        if (times != nullptr) times->allocMs += msBetween(a0, Clock::now());
    }
    if (budget == 0 || px.texture == 0) return;
    const auto t0 = Clock::now();
    const size_t rowBytes = static_cast<size_t>(px.width) * n * px.sampleBytes();
    const uint32_t remaining = px.height - px.uploadedRows;
    const uint32_t rows = static_cast<uint32_t>(std::clamp<size_t>(budget / rowBytes, 1, remaining));
    const std::vector<uint8_t>& source = px.uploadBytes ? *px.uploadBytes : *px.bytes;
    GLint previousAlignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, px.texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, static_cast<GLint>(px.uploadedRows), static_cast<GLsizei>(px.width),
                    static_cast<GLsizei>(rows), formats[n - 1], type, source.data() + px.uploadedRows * rowBytes);
    px.uploadedRows += rows;
    budget -= std::min(budget, rows * rowBytes);
    if (times != nullptr) {
        times->uploadMs += msBetween(t0, Clock::now());
        ++times->uploadFrames;
    }
    if (px.uploadedRows >= px.height) {
        const auto m0 = Clock::now();
        glGenerateMipmap(GL_TEXTURE_2D);
        if (times != nullptr) times->mipmapMs += msBetween(m0, Clock::now());
        px.textureComplete = true;
        px.uploadBytes.reset();
    }
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
}

// ---------------------------------------------------------------------------------------------
// GL: program, samplers
// ---------------------------------------------------------------------------------------------

GLuint compileStage(GLenum stage, const char* source)
{
    const GLuint shader = glCreateShader(stage);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        core::logError("[Image Viewer] shader does not compile: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool ensureProgram()
{
    if (s_program != 0) return true;
    if (s_programFailed) return false;
    // Vertex attribute locations match the ImGui OpenGL3 backend's GLSL 410 shader (0, 1, 2), whose VAO is bound.
    static const char* vs =
        "#version 410 core\n"
        "layout (location = 0) in vec2 Position;\n"
        "layout (location = 1) in vec2 UV;\n"
        "layout (location = 2) in vec4 Color;\n"
        "uniform mat4 ProjMtx;\n"
        "out vec2 Frag_UV;\n"
        "void main() { Frag_UV = UV; gl_Position = ProjMtx * vec4(Position.xy, 0.0, 1.0); }\n";
    static const char* fs =
        "#version 410 core\n"
        "in vec2 Frag_UV;\n"
        "uniform sampler2D Texture;\n"
        "uniform int u_channelCount;\n"
        "uniform int u_alphaIndex;\n"
        "uniform int u_luminance;\n"
        "uniform vec4 u_on;\n"
        "uniform vec2 u_native;\n"  // sample * x + y = stored value
        "uniform vec2 u_levels;\n"  // low, high in stored values
        "uniform int u_background;\n"
        "uniform float u_checker;\n"
        "layout (location = 0) out vec4 Out_Color;\n"
        "float level(float s) {\n"
        "    float v = s * u_native.x + u_native.y;\n"
        "    return clamp((v - u_levels.x) / max(u_levels.y - u_levels.x, 1e-20), 0.0, 1.0);\n"
        "}\n"
        "void main() {\n"
        "    vec4 s = texture(Texture, Frag_UV);\n"
        "    int shown = 0;\n"
        "    int pick = 0;\n"
        "    for (int i = 0; i < 4; ++i) { if (i < u_channelCount && u_on[i] > 0.5) { shown++; pick = i; } }\n"
        "    vec3 rgb = vec3(0.0);\n"
        "    float alpha = 1.0;\n"
        "    if (shown == 1) {\n"
        "        rgb = vec3(level(s[pick]));\n"
        "    } else if (shown > 1) {\n"
        "        for (int i = 0; i < 4; ++i) {\n"
        "            if (i >= u_channelCount || u_on[i] < 0.5 || i == u_alphaIndex) continue;\n"
        "            float v = level(s[i]);\n"
        "            if (u_luminance == 1) rgb = vec3(v); else rgb[i] = v;\n"
        "        }\n"
        "        if (u_alphaIndex >= 0 && u_on[u_alphaIndex] > 0.5) alpha = clamp(s[u_alphaIndex], 0.0, 1.0);\n"
        "    }\n"
        "    vec3 bg = vec3(0.0);\n"
        "    if (u_background == 0) {\n"
        "        vec2 cell = floor(gl_FragCoord.xy / u_checker);\n"
        "        bg = mod(cell.x + cell.y, 2.0) < 0.5 ? vec3(0.78) : vec3(0.52);\n"
        "    } else if (u_background == 1) {\n"
        "        bg = vec3(1.0);\n"
        "    } else if (u_background == 2) {\n"
        "        bg = vec3(0.5);\n"
        "    }\n"
        "    Out_Color = vec4(mix(bg, rgb, alpha), 1.0);\n"
        "}\n";
    const GLuint vertex = compileStage(GL_VERTEX_SHADER, vs);
    const GLuint fragment = compileStage(GL_FRAGMENT_SHADER, fs);
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) glDeleteShader(vertex);
        if (fragment != 0) glDeleteShader(fragment);
        s_programFailed = true;
        return false;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        core::logError("[Image Viewer] shader does not link: %s", log);
        glDeleteProgram(program);
        s_programFailed = true;
        return false;
    }
    s_program = program;
    s_locProj = glGetUniformLocation(program, "ProjMtx");
    s_locTexture = glGetUniformLocation(program, "Texture");
    s_locChannelCount = glGetUniformLocation(program, "u_channelCount");
    s_locAlphaIndex = glGetUniformLocation(program, "u_alphaIndex");
    s_locLuminance = glGetUniformLocation(program, "u_luminance");
    s_locOn = glGetUniformLocation(program, "u_on");
    s_locNative = glGetUniformLocation(program, "u_native");
    s_locLevels = glGetUniformLocation(program, "u_levels");
    s_locBackground = glGetUniformLocation(program, "u_background");
    s_locChecker = glGetUniformLocation(program, "u_checker");

    // The backend binds its own sampler object to unit 0, which overrides texture parameters; the image
    // binds one of these instead (nearest when zoomed in so the pixels are visible, linear + mipmaps below).
    glGenSamplers(1, &s_samplerNearest);
    glGenSamplers(1, &s_samplerLinear);
    for (GLuint sampler : { s_samplerNearest, s_samplerLinear }) {
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glSamplerParameteri(s_samplerNearest, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(s_samplerLinear, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return true;
}

struct DrawParams {
    GLuint sampler;
    GLint channelCount;
    GLint alphaIndex;
    GLint luminance;
    float on[4];
    float native[2];
    float levels[2];
    GLint background;
    float checker;
};

void drawCallback(const ImDrawList*, const ImDrawCmd* cmd)
{
    const DrawParams& p = *static_cast<const DrawParams*>(cmd->UserCallbackData);
    // The backend's projection: read it from its program so display position, clip origin and viewports match.
    GLint current = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    if (current != s_imguiProgram) {
        s_imguiProgram = current;
        s_imguiProjLoc = current != 0 ? glGetUniformLocation(static_cast<GLuint>(current), "ProjMtx") : -1;
    }
    GLfloat projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    if (s_imguiProjLoc >= 0) glGetUniformfv(static_cast<GLuint>(current), s_imguiProjLoc, projection);
    glUseProgram(s_program);
    glUniformMatrix4fv(s_locProj, 1, GL_FALSE, projection);
    glUniform1i(s_locTexture, 0);
    glUniform1i(s_locChannelCount, p.channelCount);
    glUniform1i(s_locAlphaIndex, p.alphaIndex);
    glUniform1i(s_locLuminance, p.luminance);
    glUniform4fv(s_locOn, 1, p.on);
    glUniform2fv(s_locNative, 1, p.native);
    glUniform2fv(s_locLevels, 1, p.levels);
    glUniform1i(s_locBackground, p.background);
    glUniform1f(s_locChecker, p.checker);
    glBindSampler(0, p.sampler);
}

// ---------------------------------------------------------------------------------------------
// Window identity
// ---------------------------------------------------------------------------------------------

std::string lowerText(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string imageKey(const ux3d::gltf::Image* img)
{
    const std::string uri = img->getUri().c_str();
    if (!uri.empty() && uri.rfind("data:", 0) != 0) {
        auto toPath = [](const std::string& text) { return std::filesystem::path(std::u8string(text.begin(), text.end())); };
        try {
            std::filesystem::path path = toPath(uri);
            if (path.is_relative()) {
                const std::string modelPath = AppSettings::instance().getCurrentModelPath();
                if (!modelPath.empty()) path = toPath(modelPath).parent_path() / path;
            }
            const auto normal = path.lexically_normal().generic_u8string();
            return "file:" + lowerText(std::string(normal.begin(), normal.end()));
        } catch (...) {
            return "file:" + lowerText(uri);
        }
    }
    if (img->getBufferView() != nullptr) return "embedded:bufferView" + std::to_string(img->m_bufferViewIndex);
    return "embedded:image" + std::to_string(img->m_ID);
}

const ux3d::gltf::Image* findImage(const ux3d::gltf::Gltf& model, const ux3d::gltf::Image* image)
{
    for (const auto& candidate : model.getImages()) {
        if (&candidate == image) return &candidate;
    }
    return nullptr;
}

const ux3d::gltf::Image* findImageByKey(const ux3d::gltf::Gltf& model, const std::string& key)
{
    for (const auto& candidate : model.getImages()) {
        if (imageKey(&candidate) == key) return &candidate;
    }
    return nullptr;
}

void resetLevels(ViewerWindow& w)
{
    const PixelSet& px = w.active();
    w.low = px.rangeMin();
    w.high = px.rangeMax();
}

void setDataLevels(ViewerWindow& w)
{
    const PixelSet& px = w.active();
    double low = std::numeric_limits<double>::infinity();
    double high = -std::numeric_limits<double>::infinity();
    bool any = false;
    for (uint32_t c = 0; c < px.channels; ++c) {
        if (!w.on[c]) continue;
        low = std::min(low, px.dataMin[c]);
        high = std::max(high, px.dataMax[c]);
        any = true;
    }
    if (!any) {
        for (uint32_t c = 0; c < px.channels; ++c) {
            low = std::min(low, px.dataMin[c]);
            high = std::max(high, px.dataMax[c]);
        }
    }
    low = std::clamp(low, px.rangeMin(), px.rangeMax());
    high = std::clamp(high, px.rangeMin(), px.rangeMax());
    if (high - low < px.step()) {
        high = low + px.step();
        if (high > px.rangeMax()) {
            high = px.rangeMax();
            low = high - px.step();
        }
    }
    w.low = low;
    w.high = high;
}

void resetView(ViewerWindow& w)
{
    w.on = { true, true, true, true };
    w.dragLine = -1;
    resetLevels(w);
    const PixelSet& px = w.active();
    w.centerX = px.width * 0.5;
    w.centerY = px.height * 0.5;
    if (w.haveReadout && (w.readX >= px.width || w.readY >= px.height)) w.haveReadout = false;
}

std::string valueText(const PixelSet& px, double v)
{
    char text[48];
    if (px.integer()) {
        std::snprintf(text, sizeof(text), "%lld", static_cast<long long>(std::llround(v)));
    } else {
        std::snprintf(text, sizeof(text), "%.4f", v);
    }
    return text;
}

ImU32 channelColor(const PixelSet& px, uint32_t c, float alpha)
{
    const char label = px.label(c);
    ImVec4 color(0.75f, 0.75f, 0.75f, alpha);
    if (label == 'R') color = ImVec4(0.90f, 0.28f, 0.28f, alpha);
    if (label == 'G') color = ImVec4(0.30f, 0.80f, 0.35f, alpha);
    if (label == 'B') color = ImVec4(0.35f, 0.52f, 0.98f, alpha);
    if (label == 'A') color = ImVec4(0.55f, 0.55f, 0.58f, alpha);
    return ImGui::ColorConvertFloat4ToU32(color);
}

void clampView(ViewerWindow& w)
{
    const PixelSet& px = w.active();
    w.zoom = std::clamp(w.zoom, kMinZoom, kMaxZoom);
    if (w.areaSize.x <= 0.0f || w.areaSize.y <= 0.0f || !px.valid()) return;
    auto clampAxis = [&](double& center, double size, float area) {
        const double halfVisible = area * 0.5 / w.zoom;
        if (size * w.zoom <= area) {
            center = size * 0.5; // smaller than the area: centred, never moves
        } else {
            center = std::clamp(center, halfVisible, size - halfVisible);
        }
    };
    clampAxis(w.centerX, px.width, w.areaSize.x);
    clampAxis(w.centerY, px.height, w.areaSize.y);
}

void setZoom(ViewerWindow& w, float zoom)
{
    w.fit = false;
    w.zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    clampView(w);
}

// ---------------------------------------------------------------------------------------------
// UV layout overlay
// ---------------------------------------------------------------------------------------------

bool primitiveUsesMaterial(const ux3d::gltf::Primitive& primitive, const ux3d::gltf::Material* material)
{
    // As the Material Inspector's UV Set row: the primitive's material, or a KHR_materials_variants mapping to it.
    if (primitive.getMaterial() == material) return true;
    return primitive.hasMaterialVariantsMapping() && primitive.getMaterialVariantsMapping()->hasMappingForMaterial(material);
}

bool materialInModel(const ux3d::gltf::Gltf& model, const ux3d::gltf::Material* material)
{
    for (const auto& candidate : model.getMaterials()) {
        if (&candidate == material) return true;
    }
    return false;
}

double elapsedMs(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

// Every triangle (TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN; indexed or not) of the primitives with the material, in
// the raw UVs of its TEXCOORD set. The drawn index slice comes from the renderer's mesh (ASOBO_primitive), else
// the whole index accessor.
void gatherUvTriangles(UvOverlay& o, const ux3d::gltf::Gltf& model, const ux3d::renderer::GltfRenderLayer* layer)
{
    namespace ma = mesh_attributes;
    const auto start = std::chrono::steady_clock::now();
    std::unordered_map<const ux3d::gltf::Primitive*, const ux3d::renderer::Mesh*> drawn;
    if (layer != nullptr) {
        for (const auto& entry : layer->m_meshMapping.map) {
            if (entry.second != nullptr) drawn.emplace(std::get<2>(entry.first), entry.second);
        }
    }
    const bool packaged = model.isExtensionDeclared("ASOBO_asset_optimized");
    const std::string semantic = "TEXCOORD_" + std::to_string(o.texCoord);
    ma::AccessorCache cache;
    o.sources.clear();
    o.triangleCount = 0;
    for (const auto& mesh : model.getMeshes()) {
        for (const auto& primitive : mesh.getPrimitives()) {
            if (!primitiveUsesMaterial(primitive, o.material)) continue;
            const auto mode = primitive.getPrimitiveMode();
            if (mode != ux3d::gltf::PrimitiveMode::Triangles && mode != ux3d::gltf::PrimitiveMode::TriangleStrip
                && mode != ux3d::gltf::PrimitiveMode::TriangleFan) {
                ++o.skippedPrimitives;
                continue;
            }
            const ux3d::gltf::Accessor* uvAccessor = ux3d::gltf::GltfUtils::getAttributeAccessor(primitive, semantic);
            if (uvAccessor == nullptr) {
                ++o.missingUvPrimitives;
                continue;
            }
            const ma::Floats uvs = cache.read(uvAccessor, 2, ma::uvIsHalfFloat(*uvAccessor, packaged));
            const ux3d::gltf::Accessor* indices = primitive.getIndicesAccessor();
            size_t indexCount = indices != nullptr ? indices->getCount() : 0;
            size_t indexByteOffset = 0;
            int64_t baseVertex = 0;
            const auto found = drawn.find(&primitive);
            if (found != drawn.end()) {
                indexCount = found->second->indexCount;
                indexByteOffset = found->second->indexByteOffset;
                baseVertex = found->second->baseVertex;
            }
            auto triangles = std::make_shared<std::vector<std::array<uint32_t, 3>>>();
            if (uvs == nullptr
                || !ma::readTriangles(primitive, indexCount, indexByteOffset, baseVertex, uvs->size() / 2, *triangles)) {
                ++o.unreadablePrimitives;
                continue;
            }
            ++o.primitives;
            o.triangleCount += triangles->size();
            o.sources.push_back(UvSource{ uvs, std::move(triangles) });
        }
    }
    o.gatherMs = elapsedMs(start);
}

struct UvEdgeKey {
    uint64_t a;
    uint64_t b;
    bool operator==(const UvEdgeKey& other) const { return a == other.a && b == other.b; }
};

uint64_t uvEdgeHash(const UvEdgeKey& k)
{
    uint64_t h = k.a * 0x9E3779B97F4A7C15ULL ^ ((k.b << 23) | (k.b >> 41)) * 0xC2B2AE3D27D4EB4FULL;
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ULL;
    return h ^ (h >> 32);
}

// Open addressing (linear probing) set of edge keys: faster and smaller than std::unordered_set on millions of edges.
// The all-ones key (two NaN end points) is the empty slot; non-finite triangles are skipped before.
class UvEdgeSet {
public:
    explicit UvEdgeSet(size_t expected)
    {
        size_t capacity = 1024;
        while (capacity < expected * 2) capacity <<= 1;
        m_slots.assign(capacity, kEmpty);
    }
    bool insert(const UvEdgeKey& key)
    {
        if ((m_size + 1) * 10 > m_slots.size() * 7) grow();
        if (place(m_slots, key)) {
            ++m_size;
            return true;
        }
        return false;
    }
    size_t size() const { return m_size; }

private:
    static constexpr UvEdgeKey kEmpty{ ~0ULL, ~0ULL };
    static bool place(std::vector<UvEdgeKey>& slots, const UvEdgeKey& key)
    {
        const size_t mask = slots.size() - 1;
        for (size_t i = uvEdgeHash(key) & mask;; i = (i + 1) & mask) {
            if (slots[i] == kEmpty) {
                slots[i] = key;
                return true;
            }
            if (slots[i] == key) return false;
        }
    }
    void grow()
    {
        std::vector<UvEdgeKey> bigger(m_slots.size() * 2, kEmpty);
        for (const UvEdgeKey& key : m_slots) {
            if (!(key == kEmpty)) place(bigger, key);
        }
        m_slots.swap(bigger);
    }
    std::vector<UvEdgeKey> m_slots;
    size_t m_size = 0;
};

uint64_t uvPointKey(float u, float v)
{
    uint32_t bu = 0;
    uint32_t bv = 0;
    if (u == 0.0f) u = 0.0f; // -0 and +0 are one point
    if (v == 0.0f) v = 0.0f;
    std::memcpy(&bu, &u, 4);
    std::memcpy(&bv, &v, 4);
    return (static_cast<uint64_t>(bu) << 32) | bv;
}

// Worker: the shown edges (and faces with Fill): the slot transform when it is applied, Wrap moves each triangle by
// the integer tile of its centroid; a shared edge (same two end points) is drawn once. Cancel is checked every
// 64k triangles.
void computeUvEdges(UvBuildJob& j)
{
    const auto start = Clock::now();
    const auto& m = j.transform;
    size_t count = 0;
    for (const UvSource& source : j.sources) count += source.triangles->size();
    j.lines.reserve(std::min(count * 2, kMaxUvEdges) * 4);
    if (j.fill) j.faces.reserve(count * 6);
    UvEdgeSet seen(std::min(count * 3 / 2 + 16, kMaxUvEdges));
    size_t done = 0;
    for (const UvSource& source : j.sources) {
        const std::vector<float>& uv = *source.uvs;
        for (const auto& triangle : *source.triangles) {
            if ((++done & 0xFFFF) == 0 && j.cancel.load(std::memory_order_relaxed)) return;
            float p[3][2];
            for (int i = 0; i < 3; ++i) {
                const double u = uv[static_cast<size_t>(triangle[static_cast<size_t>(i)]) * 2];
                const double v = uv[static_cast<size_t>(triangle[static_cast<size_t>(i)]) * 2 + 1];
                if (j.applyTransform) {
                    p[i][0] = static_cast<float>(m[0] * u + m[3] * v + m[6]);
                    p[i][1] = static_cast<float>(m[1] * u + m[4] * v + m[7]);
                } else {
                    p[i][0] = static_cast<float>(u);
                    p[i][1] = static_cast<float>(v);
                }
            }
            if (!std::isfinite(p[0][0] + p[0][1] + p[1][0] + p[1][1] + p[2][0] + p[2][1])) continue; // NaN / inf UVs
            bool out = false;
            for (const auto& point : p) {
                if (!(point[0] >= 0.0f && point[0] <= 1.0f && point[1] >= 0.0f && point[1] <= 1.0f)) out = true;
            }
            if (out) ++j.outside;
            if (j.wrap) {
                const float tileU = std::floor((p[0][0] + p[1][0] + p[2][0]) / 3.0f);
                const float tileV = std::floor((p[0][1] + p[1][1] + p[2][1]) / 3.0f);
                if (std::isfinite(tileU) && std::isfinite(tileV) && (tileU != 0.0f || tileV != 0.0f)) {
                    for (auto& point : p) {
                        point[0] -= tileU;
                        point[1] -= tileV;
                    }
                }
            }
            if (j.fill) {
                for (const auto& point : p) {
                    j.faces.push_back(point[0]);
                    j.faces.push_back(point[1]);
                }
            }
            if (j.capped) continue;
            for (int e = 0; e < 3; ++e) {
                const float* a = p[e];
                const float* b = p[(e + 1) % 3];
                uint64_t ka = uvPointKey(a[0], a[1]);
                uint64_t kb = uvPointKey(b[0], b[1]);
                if (ka == kb) continue; // degenerate edge
                if (kb < ka) std::swap(ka, kb);
                if (!seen.insert(UvEdgeKey{ ka, kb })) continue;
                if (seen.size() > kMaxUvEdges) {
                    j.capped = true;
                    break;
                }
                j.lines.insert(j.lines.end(), { a[0], a[1], b[0], b[1] });
            }
        }
    }
    j.buildMs = msBetween(start, Clock::now());
}

void runUvBuild(UvBuildJob& j)
{
    lowerThreadPriority();
    try {
        computeUvEdges(j);
    } catch (const std::bad_alloc&) {
        j.lines.clear();
        j.faces.clear();
        j.capped = true;
    }
    j.finished.store(true, std::memory_order_release);
}

// Main thread: the finished build replaces the drawn buffers.
void adoptUvBuild(UvOverlay& o, UvBuildJob& j)
{
    const auto t0 = Clock::now();
    GLint previousBuffer = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);
    if (o.lineBuffer == 0) glGenBuffers(1, &o.lineBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, o.lineBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(j.lines.size() * sizeof(float)),
                 j.lines.empty() ? nullptr : j.lines.data(), GL_STATIC_DRAW);
    if (j.fill) {
        if (o.fillBuffer == 0) glGenBuffers(1, &o.fillBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, o.fillBuffer);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(j.faces.size() * sizeof(float)),
                     j.faces.empty() ? nullptr : j.faces.data(), GL_STATIC_DRAW);
    } else if (o.fillBuffer != 0) {
        glDeleteBuffers(1, &o.fillBuffer);
        o.fillBuffer = 0;
    }
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousBuffer));
    o.edges = j.lines.size() / 4;
    o.fillTriangles = j.faces.size() / 6;
    o.outside = j.outside;
    o.capped = j.capped;
    o.buildMs = j.buildMs;
    o.built = true;
    o.builtWrap = j.wrap;
    o.builtTransform = j.transformOption;
    o.builtFill = j.fill;
    o.bufferUploadMs = msBetween(t0, Clock::now());
    if (o.capped) {
        core::logWarning("[Image Viewer] UV layout of '%s': more than %zu edges; the rest is not drawn", o.materialName.c_str(),
                         kMaxUvEdges);
    }
}

bool uvBuildWanted(const UvOverlay& o)
{
    return !o.built || o.builtWrap != o.wrap || o.builtTransform != o.applyTransform || (o.fill && !o.builtFill);
}

// Main thread, every frame: starts a build on a worker when Wrap / Transform / first Fill changed (a build for an
// older choice is cancelled), and adopts a finished one. The old buffers stay drawn until then.
void updateUvBuild(UvOverlay& o)
{
    if (o.job && o.job->finished.load(std::memory_order_acquire)) {
        adoptUvBuild(o, *o.job);
        joinTask(o.job);
    }
    if (!uvBuildWanted(o)) return;
    if (o.job && o.job->wrap == o.wrap && o.job->transformOption == o.applyTransform && (o.job->fill || !o.fill)) return;
    retireTask(o.job);
    auto job = std::make_shared<UvBuildJob>();
    job->sources = o.sources;
    job->transform = o.transform;
    job->applyTransform = o.applyTransform && o.hasTransform;
    job->transformOption = o.applyTransform;
    job->wrap = o.wrap;
    job->fill = o.fill;
    UvBuildJob* raw = job.get();
    job->thread = std::thread([raw] { runUvBuild(*raw); });
    o.job = std::move(job);
    o.buildMaxViewerMs = 0.0;
    o.buildMaxFrameMs = 0.0;
}

bool uvBusy(const UvOverlay& o)
{
    return o.job != nullptr || uvBuildWanted(o);
}

// decoding -> uploading -> histogram -> uv -> ready ("ready" also for an error, which is final).
std::string loadPhase(const ViewerWindow& w)
{
    if (!w.pixelsReady) return w.job && w.job->copying ? "reading" : "decoding";
    if (!w.error.empty()) return "ready";
    const PixelSet& px = w.active();
    if (!px.textureComplete && px.uploadError.empty()) return "uploading";
    if (!w.statsReady) return "histogram";
    if (w.uv && uvBusy(*w.uv)) return "uv";
    return "ready";
}

GLuint s_uvProgram = 0;
bool s_uvProgramFailed = false;
GLint s_uvLocProj = -1, s_uvLocMap = -1, s_uvLocColor = -1;

bool ensureUvProgram()
{
    if (s_uvProgram != 0) return true;
    if (s_uvProgramFailed) return false;
    static const char* vs =
        "#version 410 core\n"
        "layout (location = 0) in vec2 UvPosition;\n"
        "uniform mat4 ProjMtx;\n"
        "uniform vec4 u_map;\n" // screen position = u_map.xy + uv * u_map.zw (image row 0 at the top = glTF v 0)
        "void main() { gl_Position = ProjMtx * vec4(u_map.xy + UvPosition * u_map.zw, 0.0, 1.0); }\n";
    static const char* fs =
        "#version 410 core\n"
        "uniform vec4 u_color;\n"
        "layout (location = 0) out vec4 Out_Color;\n"
        "void main() { Out_Color = u_color; }\n";
    const GLuint vertex = compileStage(GL_VERTEX_SHADER, vs);
    const GLuint fragment = compileStage(GL_FRAGMENT_SHADER, fs);
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) glDeleteShader(vertex);
        if (fragment != 0) glDeleteShader(fragment);
        s_uvProgramFailed = true;
        return false;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        glDeleteProgram(program);
        s_uvProgramFailed = true;
        core::logError("[Image Viewer] UV layout shader does not link");
        return false;
    }
    s_uvProgram = program;
    s_uvLocProj = glGetUniformLocation(program, "ProjMtx");
    s_uvLocMap = glGetUniformLocation(program, "u_map");
    s_uvLocColor = glGetUniformLocation(program, "u_color");
    return true;
}

struct UvDrawParams {
    GLuint lineBuffer;
    GLuint fillBuffer;
    GLsizei lineVertices;
    GLsizei fillVertices;
    float map[4];
    float lineColor[4];
    float fillColor[4];
};

// Draws the overlay's buffers right away (the backend state was reset before this callback, so its program and
// projection are current). The scissor is set from the command's clip rectangle through the projection and the
// GL viewport, so it is right in every OS window. The VAO is made for this draw only: VAOs are not shared between
// the GL contexts of detached windows; buffers and programs are.
void uvDrawCallback(const ImDrawList*, const ImDrawCmd* cmd)
{
    const UvDrawParams& p = *static_cast<const UvDrawParams*>(cmd->UserCallbackData);
    GLint current = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    GLfloat projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    const GLint projLoc = current != 0 ? glGetUniformLocation(static_cast<GLuint>(current), "ProjMtx") : -1;
    if (projLoc >= 0) glGetUniformfv(static_cast<GLuint>(current), projLoc, projection);

    GLint viewport[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_VIEWPORT, viewport);
    auto toPixel = [&](float x, float y, float& px, float& py) {
        const float nx = projection[0] * x + projection[4] * y + projection[12];
        const float ny = projection[1] * x + projection[5] * y + projection[13];
        px = static_cast<float>(viewport[0]) + (nx * 0.5f + 0.5f) * static_cast<float>(viewport[2]);
        py = static_cast<float>(viewport[1]) + (ny * 0.5f + 0.5f) * static_cast<float>(viewport[3]);
    };
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    toPixel(cmd->ClipRect.x, cmd->ClipRect.y, x0, y0);
    toPixel(cmd->ClipRect.z, cmd->ClipRect.w, x1, y1);
    const int sx = static_cast<int>(std::floor(std::min(x0, x1) + 0.5f));
    const int sy = static_cast<int>(std::floor(std::min(y0, y1) + 0.5f));
    const int sw = static_cast<int>(std::floor(std::max(x0, x1) + 0.5f)) - sx;
    const int sh = static_cast<int>(std::floor(std::max(y0, y1) + 0.5f)) - sy;
    if (sw <= 0 || sh <= 0) return;
    glEnable(GL_SCISSOR_TEST);
    glScissor(sx, sy, sw, sh);

    glUseProgram(s_uvProgram);
    glUniformMatrix4fv(s_uvLocProj, 1, GL_FALSE, projection);
    glUniform4fv(s_uvLocMap, 1, p.map);
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glEnableVertexAttribArray(0);
    if (p.fillBuffer != 0 && p.fillVertices > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, p.fillBuffer);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glUniform4fv(s_uvLocColor, 1, p.fillColor);
        glDrawArrays(GL_TRIANGLES, 0, p.fillVertices);
    }
    if (p.lineBuffer != 0 && p.lineVertices > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, p.lineBuffer);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glUniform4fv(s_uvLocColor, 1, p.lineColor);
        glDrawArrays(GL_LINES, 0, p.lineVertices);
    }
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

void drawUvOverlay(ViewerWindow& w, ImDrawList* dl, ImVec2 imageMin, ImVec2 imageMax)
{
    if (!w.uv || !w.uv->show || !ensureUvProgram()) return;
    UvOverlay& o = *w.uv;
    if (!o.built) return;
    if (o.edges == 0 && (!o.fill || o.fillTriangles == 0)) return;
    UvDrawParams params{};
    params.lineBuffer = o.lineBuffer;
    params.fillBuffer = o.fill ? o.fillBuffer : 0;
    params.lineVertices = static_cast<GLsizei>(o.edges * 2);
    params.fillVertices = o.fill ? static_cast<GLsizei>(o.fillTriangles * 3) : 0;
    params.map[0] = imageMin.x;
    params.map[1] = imageMin.y;
    params.map[2] = imageMax.x - imageMin.x;
    params.map[3] = imageMax.y - imageMin.y;
    const float alpha = std::clamp(o.opacity, 0.05f, 1.0f);
    const float line[4] = { 0.93f, 0.93f, 0.96f, alpha };
    const float face[4] = { 0.80f, 0.84f, 1.0f, alpha * 0.22f };
    std::memcpy(params.lineColor, line, sizeof(line));
    std::memcpy(params.fillColor, face, sizeof(face));
    dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
    dl->AddCallback(uvDrawCallback, &params, sizeof(params));
    dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
}

// The second status row while an overlay is attached: UV toggle, material and TEXCOORD set, Wrap, Fill, Transform
// (when the slot has one), opacity and the counts (right-aligned when they fit).
void drawUvStatusRow(ViewerWindow& w)
{
    UvOverlay& o = *w.uv;
    const float rowEndX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (o.show) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button("UV")) o.show = !o.show;
    if (o.show) ImGui::PopStyleColor();
    UiProbe::item("imageViewer.uv", w.imageIndex);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s the UV layout overlay", o.show ? "Hide" : "Show");
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s  TEXCOORD_%u", o.materialName.c_str(), o.texCoord);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Material %u \"%s\", %s\nThe triangle edges of every primitive with this material in TEXCOORD_%u.",
                          o.materialIndex, o.materialName.c_str(), o.slot.c_str(), o.texCoord);
    }
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::Checkbox("Wrap", &o.wrap);
    UiProbe::item("imageViewer.uv.wrap", w.imageIndex);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move each triangle into 0..1 by the integer tile of its centre (tiled and detail UVs)");
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::Checkbox("Fill", &o.fill);
    UiProbe::item("imageViewer.uv.fill", w.imageIndex);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("A faint face fill (overlapping faces add up)");
    if (o.hasTransform) {
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::Checkbox("Transform", &o.applyTransform);
        UiProbe::item("imageViewer.uv.transform", w.imageIndex);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("On: the slot's KHR_texture_transform (offset, rotation, scale), as the texture is sampled.\nOff: the raw UVs.");
        }
    }
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::SetNextItemWidth(70.0f);
    ImGui::SliderFloat("##uvOpacity", &o.opacity, 0.05f, 1.0f, "%.2f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line opacity");

    char counts[192];
    std::snprintf(counts, sizeof(counts), "%u prim  %zu tris  %llu outside 0..1", o.primitives, o.triangleCount,
                  static_cast<unsigned long long>(o.outside));
    std::string text = counts;
    const bool note = o.skippedPrimitives > 0 || o.missingUvPrimitives > 0 || o.unreadablePrimitives > 0 || o.capped;
    const float width = ImGui::CalcTextSize(text.c_str()).x + (note ? ImGui::CalcTextSize("(!) ").x : 0.0f);
    ImGui::SameLine(0.0f, 16.0f);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    if (cursor.x + width <= rowEndX) ImGui::SetCursorScreenPos(ImVec2(rowEndX - width, cursor.y));
    ImGui::BeginGroup();
    if (note) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "(!)");
        ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
    }
    ImGui::TextDisabled("%s", text.c_str());
    ImGui::EndGroup();
    if (ImGui::IsItemHovered()) {
        std::string tip = std::to_string(o.primitives) + " triangle primitives, " + std::to_string(o.triangleCount)
            + " triangles, " + std::to_string(o.edges) + " edges drawn, " + std::to_string(o.outside)
            + " triangles partly outside 0..1" + (o.applyTransform && o.hasTransform ? " (after the transform)" : "");
        if (o.skippedPrimitives > 0) tip += "\n" + std::to_string(o.skippedPrimitives) + " point / line primitives skipped";
        if (o.missingUvPrimitives > 0) {
            tip += "\n" + std::to_string(o.missingUvPrimitives) + " primitives without TEXCOORD_" + std::to_string(o.texCoord);
        }
        if (o.unreadablePrimitives > 0) tip += "\n" + std::to_string(o.unreadablePrimitives) + " primitives with unreadable UVs or indices";
        if (o.capped) tip += "\nOver " + std::to_string(kMaxUvEdges) + " edges: the rest is not drawn";
        char timing[96];
        std::snprintf(timing, sizeof(timing), "\nRead %.1f ms, built %.1f ms", o.gatherMs, o.buildMs);
        tip += timing;
        ImGui::SetTooltip("%s", tip.c_str());
    }
}

nlohmann::json uvStateJson(const UvOverlay& o)
{
    return {
        { "material", o.materialIndex },
        { "materialName", o.materialName },
        { "slot", o.slot },
        { "texCoord", o.texCoord },
        { "hasTransform", o.hasTransform },
        { "transformApplied", o.applyTransform && o.hasTransform },
        { "transform", o.transform },
        { "show", o.show },
        { "wrap", o.wrap },
        { "fill", o.fill },
        { "opacity", o.opacity },
        { "primitives", o.primitives },
        { "triangles", o.triangleCount },
        { "building", uvBusy(o) },
        { "bufferUploadMs", o.bufferUploadMs },
        { "buildLongestViewerFrameMs", o.buildMaxViewerMs },
        { "buildLongestFrameMs", o.buildMaxFrameMs },
        { "outside", o.outside },
        { "edges", o.edges },
        { "fillTriangles", o.fillTriangles },
        { "skippedPrimitives", o.skippedPrimitives },
        { "missingUvPrimitives", o.missingUvPrimitives },
        { "unreadablePrimitives", o.unreadablePrimitives },
        { "capped", o.capped },
        { "gatherMs", o.gatherMs },
        { "buildMs", o.buildMs },
    };
}

// ---------------------------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------------------------

void drawSwatchBackground(ImDrawList* dl, ImVec2 min, ImVec2 max, int background)
{
    if (background == 0) {
        const float cell = std::max(2.0f, (max.x - min.x) * 0.5f);
        for (int iy = 0; iy < 2; ++iy) {
            for (int ix = 0; ix < 2; ++ix) {
                const ImVec2 a(min.x + ix * cell, min.y + iy * cell);
                const ImVec2 b(std::min(max.x, a.x + cell), std::min(max.y, a.y + cell));
                const int shade = ((ix + iy) & 1) == 0 ? 199 : 133;
                dl->AddRectFilled(a, b, IM_COL32(shade, shade, shade, 255));
            }
        }
        return;
    }
    const int shade = background == 1 ? 255 : background == 2 ? 128 : 0;
    dl->AddRectFilled(min, max, IM_COL32(shade, shade, shade, 255));
}

// The colour picker: the full-colour swatch spanning every row, then one row per channel with its label, a level bar
// (value / maximum of the bit depth, in the channel colour) and the stored value. Hidden channels are dimmed; it
// keeps the last pixel when the cursor leaves the image and shows empty bars before the first one.
void drawPicker(ViewerWindow& w, ImVec2 size)
{
    const PixelSet& px = w.active();
    const ImVec2 min = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);
    if (!w.pixelsReady) {
        // No pixel values yet: the readout comes with the full-resolution data.
        ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 8.0f, min.y + 8.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                            "Readout: loading the full-resolution pixels...");
        return;
    }
    if (ImGui::IsItemHovered()) {
        if (w.haveReadout) {
            ImGui::SetTooltip("Pixel x %u  y %u (stored values; the 5x5 grid around it is composited over the background)", w.readX, w.readY);
        } else {
            ImGui::SetTooltip("Hover the image to read a pixel");
        }
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool have = w.haveReadout && w.readX < px.width && w.readY < px.height;
    std::array<double, 4> stored{};
    std::array<double, 4> level{};
    if (have) {
        for (uint32_t c = 0; c < px.channels; ++c) {
            stored[c] = px.value(w.readX, w.readY, c);
            level[c] = px.normalized(stored[c]);
        }
    }

    // A 5x5 neighbourhood around the read pixel (centre cell), each cell the pixel's full colour over the chosen
    // background with its stored alpha, separated by a thin grid. Cells outside the image stay empty (dark).
    constexpr int kGrid = 5;
    const float swatch = std::floor(size.y / kGrid) * kGrid;
    const float cell = swatch / kGrid;
    const ImVec2 swatchMax(min.x + swatch, min.y + swatch);
    const ImU32 emptyColor = IM_COL32(24, 24, 28, 255);
    dl->AddRectFilled(min, swatchMax, emptyColor);
    auto pixelColor = [&](uint32_t x, uint32_t y) {
        std::array<float, 4> l{};
        for (uint32_t c = 0; c < px.channels && c < 4; ++c) l[c] = static_cast<float>(px.normalized(px.value(x, y, c)));
        float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
        if (px.luminance) {
            r = g = b = l[0];
        } else {
            r = l[0];
            g = px.channels > 1 ? l[1] : 0.0f;
            b = px.channels > 2 ? l[2] : 0.0f;
        }
        if (px.alphaIndex() >= 0) a = l[static_cast<size_t>(px.alphaIndex())];
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
    };
    if (have) {
        for (int gy = 0; gy < kGrid; ++gy) {
            for (int gx = 0; gx < kGrid; ++gx) {
                const int64_t x = static_cast<int64_t>(w.readX) + gx - kGrid / 2;
                const int64_t y = static_cast<int64_t>(w.readY) + gy - kGrid / 2;
                if (x < 0 || y < 0 || x >= static_cast<int64_t>(px.width) || y >= static_cast<int64_t>(px.height)) continue;
                const ImVec2 a(min.x + gx * cell, min.y + gy * cell);
                const ImVec2 b(a.x + cell, a.y + cell);
                drawSwatchBackground(dl, a, b, w.background);
                dl->AddRectFilled(a, b, pixelColor(static_cast<uint32_t>(x), static_cast<uint32_t>(y)));
            }
        }
    }
    const ImU32 gridColor = IM_COL32(60, 60, 66, 255);
    for (int i = 1; i < kGrid; ++i) {
        const float x = min.x + i * cell;
        const float y = min.y + i * cell;
        dl->AddLine(ImVec2(x, min.y), ImVec2(x, swatchMax.y), gridColor, 1.0f);
        dl->AddLine(ImVec2(min.x, y), ImVec2(swatchMax.x, y), gridColor, 1.0f);
    }
    dl->AddRect(min, swatchMax, IM_COL32(90, 90, 98, 255));
    if (have) {
        // The read pixel: a two-tone outline that stays visible on light and dark colours.
        const ImVec2 c0(min.x + (kGrid / 2) * cell, min.y + (kGrid / 2) * cell);
        const ImVec2 c1(c0.x + cell, c0.y + cell);
        dl->AddRect(ImVec2(c0.x - 1.0f, c0.y - 1.0f), ImVec2(c1.x + 1.0f, c1.y + 1.0f), IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);
        dl->AddRect(c0, c1, IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.0f);
    }

    // One row per channel (rows of a quarter of the height, top aligned).
    const ImGuiStyle& style = ImGui::GetStyle();
    const float rowsX = swatchMax.x + style.ItemSpacing.x;
    const float rowHeight = size.y / 4.0f;
    const float labelWidth = ImGui::CalcTextSize("W").x + style.ItemInnerSpacing.x;
    const char* widest = px.type == SampleType::F32 ? "-0.0000"
                       : px.type == SampleType::U16 ? "65535"
                       : px.type == SampleType::S8  ? "-128"
                                                    : "255";
    const float valueWidth = ImGui::CalcTextSize(widest).x;
    const float barX0 = rowsX + labelWidth;
    const float barX1 = std::max(barX0 + 8.0f, min.x + size.x - valueWidth - style.ItemInnerSpacing.x);
    const float textHeight = ImGui::GetTextLineHeight();
    for (uint32_t c = 0; c < px.channels; ++c) {
        const float y0 = min.y + rowHeight * static_cast<float>(c);
        const float textY = y0 + (rowHeight - textHeight) * 0.5f;
        const bool shown = w.on[c];
        const float dim = shown ? 1.0f : 0.35f;
        const char label[2] = { px.label(c), 0 };
        dl->AddText(ImVec2(rowsX, textY), channelColor(px, c, dim), label);
        const float barH = std::max(3.0f, rowHeight * 0.45f);
        const float barY0 = y0 + (rowHeight - barH) * 0.5f;
        dl->AddRectFilled(ImVec2(barX0, barY0), ImVec2(barX1, barY0 + barH), IM_COL32(28, 28, 33, 255), 2.0f);
        if (have) {
            const float fill = barX0 + (barX1 - barX0) * static_cast<float>(level[c]);
            if (fill > barX0) {
                dl->AddRectFilled(ImVec2(barX0, barY0), ImVec2(fill, barY0 + barH), channelColor(px, c, shown ? 0.95f : 0.30f), 2.0f);
            }
            const std::string text = valueText(px, stored[c]);
            const float textW = ImGui::CalcTextSize(text.c_str()).x;
            dl->AddText(ImVec2(min.x + size.x - textW, textY), ImGui::GetColorU32(ImGuiCol_Text, dim), text.c_str());
        }
    }
}

void drawHistogram(ViewerWindow& w, ImVec2 size)
{
    PixelSet& px = w.active();
    ImGui::InvisibleButton("##histogram", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    UiProbe::item("imageViewer.histogram", w.imageIndex);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float width = max.x - min.x;
    const float height = max.y - min.y;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(min, max, IM_COL32(16, 16, 20, 255));
    if (!w.pixelsReady || !w.statsReady) {
        const char* text = "Computing histogram...";
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        dl->AddText(ImVec2(min.x + (width - textSize.x) * 0.5f, min.y + (height - textSize.y) * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
        dl->AddRect(min, max, IM_COL32(70, 70, 78, 255));
        if (!w.pixelsReady) return; // the levels need the pixel type
    }

    const double range = px.rangeMax() - px.rangeMin();
    auto valueToX = [&](double v) { return min.x + static_cast<float>((v - px.rangeMin()) / range) * width; };
    auto xToValue = [&](float x) {
        double v = px.rangeMin() + std::clamp((x - min.x) / std::max(width, 1.0f), 0.0f, 1.0f) * range;
        if (px.integer()) v = std::round(v);
        return v;
    };

    // Square-root heights: one spike (a flat background, alpha 255) would otherwise flatten everything else.
    uint32_t tallest = 0;
    for (uint32_t c = 0; c < px.channels; ++c) {
        if (!w.on[c]) continue;
        for (uint32_t count : px.histogram[c]) tallest = std::max(tallest, count);
    }
    if (tallest > 0) {
        const float norm = 1.0f / std::sqrt(static_cast<float>(tallest));
        const float binWidth = width / static_cast<float>(kBins);
        for (uint32_t c = 0; c < px.channels; ++c) {
            if (!w.on[c]) continue;
            const ImU32 color = channelColor(px, c, 0.45f);
            for (int b = 0; b < kBins; ++b) {
                const uint32_t count = px.histogram[c][static_cast<size_t>(b)];
                if (count == 0) continue;
                const float h = std::sqrt(static_cast<float>(count)) * norm * (height - 2.0f);
                const float x0 = min.x + static_cast<float>(b) * binWidth;
                dl->AddRectFilled(ImVec2(x0, max.y - 1.0f - h), ImVec2(x0 + std::max(binWidth, 1.0f), max.y - 1.0f), color);
            }
        }
    }

    // Interaction: a drag moves the nearer line (a click alone does not move anything).
    const float mouseX = ImGui::GetIO().MousePos.x;
    const float xLow = valueToX(w.low);
    const float xHigh = valueToX(w.high);
    if (ImGui::IsItemActivated() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        w.dragLine = std::abs(mouseX - xLow) <= std::abs(mouseX - xHigh) ? 0 : 1;
        if (w.high - w.low <= px.step() && mouseX > xHigh) w.dragLine = 1;
    }
    if (!active) w.dragLine = -1;
    if (active && w.dragLine >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
        const double v = xToValue(mouseX);
        if (w.dragLine == 0) {
            w.low = std::clamp(v, px.rangeMin(), w.high - px.step());
        } else {
            w.high = std::clamp(v, w.low + px.step(), px.rangeMax());
        }
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        resetLevels(w);
        w.dragLine = -1;
    }
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) ImGui::OpenPopup("##histogramMenu");
    if (ImGui::BeginPopup("##histogramMenu")) {
        const std::string fullLabel = px.type == SampleType::S8
            ? "Full range (-128 to 127)"
            : "Full range (0-" + valueText(px, px.rangeMax()) + ")";
        if (ImGui::MenuItem(fullLabel.c_str())) resetLevels(w);
        if (ImGui::MenuItem("Data range (min-max of shown channels)")) setDataLevels(w);
        ImGui::EndPopup();
    }

    // Levels: the parts outside [low, high] are dimmed, the two lines drawn on top.
    const float lowX = valueToX(w.low);
    const float highX = valueToX(w.high);
    dl->AddRectFilled(min, ImVec2(lowX, max.y), IM_COL32(0, 0, 0, 110));
    dl->AddRectFilled(ImVec2(highX, min.y), max, IM_COL32(0, 0, 0, 110));
    const ImU32 lineColor = IM_COL32(250, 210, 80, 255);
    dl->AddLine(ImVec2(lowX, min.y), ImVec2(lowX, max.y), lineColor, w.dragLine == 0 ? 2.5f : 1.5f);
    dl->AddLine(ImVec2(highX, min.y), ImVec2(highX, max.y), lineColor, w.dragLine == 1 ? 2.5f : 1.5f);
    dl->AddRect(min, max, IM_COL32(70, 70, 78, 255));

    if (w.dragLine >= 0) {
        ImGui::SetTooltip("%s: %s", w.dragLine == 0 ? "Low" : "High", valueText(px, w.dragLine == 0 ? w.low : w.high).c_str());
    } else if (hovered && !ImGui::IsPopupOpen("##histogramMenu")) {
        const float grab = 6.0f;
        if (std::abs(mouseX - lowX) <= grab) {
            ImGui::SetTooltip("Low: %s", valueText(px, w.low).c_str());
        } else if (std::abs(mouseX - highX) <= grab) {
            ImGui::SetTooltip("High: %s", valueText(px, w.high).c_str());
        } else {
            // The histogram bin under the cursor, binned the same way computeStats fills it.
            const double value = xToValue(mouseX);
            int bin = 0;
            switch (px.type) {
            case SampleType::U8: bin = static_cast<int>(value); break;
            case SampleType::S8: bin = static_cast<int>(value) + 128; break;
            case SampleType::U16: bin = static_cast<int>(value) >> 8; break;
            default: bin = static_cast<int>(std::floor(std::clamp(value, 0.0, 1.0) * kBins)); break;
            }
            bin = std::clamp(bin, 0, kBins - 1);
            const float binX0 = min.x + static_cast<float>(bin) * width / static_cast<float>(kBins);
            const float binX1 = min.x + static_cast<float>(bin + 1) * width / static_cast<float>(kBins);
            dl->AddRectFilled(ImVec2(binX0, min.y + 1.0f), ImVec2(std::max(binX1, binX0 + 1.0f), max.y - 1.0f),
                              IM_COL32(255, 255, 255, 40));

            ImGui::BeginTooltip();
            if (px.type == SampleType::U16) {
                ImGui::Text("Value %s  (bin %d-%d)", valueText(px, value).c_str(), bin * 256, bin * 256 + 255);
            } else if (px.type == SampleType::F32) {
                ImGui::Text("Value %s  (bin %.4f-%.4f)", valueText(px, value).c_str(),
                            static_cast<double>(bin) / kBins, static_cast<double>(bin + 1) / kBins);
            } else {
                ImGui::Text("Value %s", valueText(px, value).c_str());
            }
            if (w.statsReady) {
                const double total = std::max(1.0, static_cast<double>(px.width) * static_cast<double>(px.height));
                for (uint32_t c = 0; c < px.channels; ++c) {
                    const uint32_t count = px.histogram[c][static_cast<size_t>(bin)];
                    const char label[2] = { px.label(c), 0 };
                    const float dim = w.on[c] ? 1.0f : 0.4f;
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(channelColor(px, c, dim)), "%s", label);
                    ImGui::SameLine();
                    ImGui::TextColored(ImGui::GetStyleColorVec4(w.on[c] ? ImGuiCol_Text : ImGuiCol_TextDisabled),
                                       "%10u px  %7.3f%%", count, 100.0 * count / total);
                }
            }
            ImGui::Separator();
            ImGui::TextDisabled("Levels %s - %s", valueText(px, w.low).c_str(), valueText(px, w.high).c_str());
            ImGui::TextDisabled("Drag to move the nearer line; double-click: full range;\n"
                                "right-click: presets. Heights are square-root scaled.");
            ImGui::EndTooltip();
        }
    }
}

// Picker | splitter | histogram. The split is a fraction of the width for this run only (never stored).
float s_splitFraction = 0.42f;

void drawTopBar(ViewerWindow& w, float height)
{
    const float width = ImGui::GetContentRegionAvail().x;
    const float splitterWidth = 6.0f;
    const float minPicker = 180.0f;
    const float minHistogram = 200.0f;
    float pickerWidth = std::floor(width * s_splitFraction);
    if (width - splitterWidth >= minPicker + minHistogram) {
        pickerWidth = std::clamp(pickerWidth, minPicker, width - splitterWidth - minHistogram);
    } else {
        pickerWidth = std::max(1.0f, (width - splitterWidth) * 0.5f);
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    drawPicker(w, ImVec2(pickerWidth, height));

    // Vertical splitter: drag to move the boundary.
    ImGui::SetCursorScreenPos(ImVec2(origin.x + pickerWidth, origin.y));
    ImGui::InvisibleButton("##split", ImVec2(splitterWidth, height));
    UiProbe::item("imageViewer.splitter", w.imageIndex);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    if (hovered || active) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (active && width > 0.0f) {
        const float wanted = ImGui::GetIO().MousePos.x - origin.x - splitterWidth * 0.5f;
        s_splitFraction = std::clamp(wanted / width, 0.0f, 1.0f);
    }
    const float lineX = origin.x + pickerWidth + splitterWidth * 0.5f;
    const ImGuiCol lineColor = active ? ImGuiCol_SeparatorActive : hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(lineX, origin.y + 2.0f), ImVec2(lineX, origin.y + height - 2.0f),
                                        ImGui::GetColorU32(lineColor), 2.0f);

    ImGui::SetCursorScreenPos(ImVec2(origin.x + pickerWidth + splitterWidth, origin.y));
    drawHistogram(w, ImVec2(std::max(1.0f, width - pickerWidth - splitterWidth), height));
}

// One row under the image: zoom, channel toggles, background, Raw, the cursor pixel and the image facts (right).
// ---------------------------------------------------------------------------------------------
// Crosshair and pixel grid (one set of preferences for every viewer, [ImageViewer] in settings.ini)
// ---------------------------------------------------------------------------------------------

struct GridLevel {
    std::array<float, 4> color{ 1.0f, 1.0f, 1.0f, 0.5f };
    int spacing = 256; // image pixels
    bool operator==(const GridLevel& o) const { return color == o.color && spacing == o.spacing; }
};

struct ViewerPrefs {
    bool crosshair = false;
    bool grid = false;
    GridLevel major{ { 1.0f, 1.0f, 1.0f, 0.5f }, 256 };
    GridLevel minor{ { 1.0f, 1.0f, 1.0f, 0.25f }, 64 };
};

constexpr int kMinGridSpacing = 1;
constexpr int kMaxGridSpacing = 16384;
constexpr float kMinGridScreenSpacing = 4.0f; // a level closer than this on screen is hidden
constexpr const char* kPrefsSection = "ImageViewer";

ViewerPrefs s_prefs;      // in effect (UI and scripts)
ViewerPrefs s_savedPrefs; // what settings.ini holds (script edits without set_persist stay out of it)
bool s_prefsLoaded = false;
bool s_prefsDirty = false;

std::string colorText(const std::array<float, 4>& c)
{
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%.4g,%.4g,%.4g,%.4g", c[0], c[1], c[2], c[3]);
    return buffer;
}

bool parseColor(const std::string& text, std::array<float, 4>& out)
{
    std::array<float, 4> c{};
    char tail = 0;
    if (std::sscanf(text.c_str(), "%f,%f,%f,%f%c", &c[0], &c[1], &c[2], &c[3], &tail) != 4) return false;
    for (float v : c) {
        if (!std::isfinite(v) || v < 0.0f || v > 1.0f) return false;
    }
    out = c;
    return true;
}

bool parseSpacing(const std::string& text, int& out)
{
    char* end = nullptr;
    const long v = std::strtol(text.c_str(), &end, 10);
    if (text.empty() || end == nullptr || *end != 0 || v < kMinGridSpacing || v > kMaxGridSpacing) return false;
    out = static_cast<int>(v);
    return true;
}

// Once, at the first use: reads [ImageViewer] (never with --isolated). A value that does not parse keeps its default
// and logs one warning.
void ensurePrefsLoaded()
{
    if (s_prefsLoaded) return;
    s_prefsLoaded = true;
    SettingsFile& settings = SettingsFile::instance();
    if (!settings.persistenceEnabled()) return;
    settings.load();
    ViewerPrefs p;
    auto warn = [](const char* key, const std::string& value) {
        core::logWarning("[Image Viewer] settings.ini [ImageViewer] %s=%s is not valid; using the default", key, value.c_str());
    };
    auto readBool = [&](const char* key, bool& value) {
        if (!settings.hasKey(kPrefsSection, key)) return;
        const std::string v = lowerText(settings.get(kPrefsSection, key));
        if (v == "1" || v == "true" || v == "on") value = true;
        else if (v == "0" || v == "false" || v == "off") value = false;
        else warn(key, v);
    };
    auto readLevel = [&](const char* colorKey, const char* spacingKey, GridLevel& level) {
        if (settings.hasKey(kPrefsSection, colorKey)) {
            const std::string v = settings.get(kPrefsSection, colorKey);
            if (!parseColor(v, level.color)) warn(colorKey, v);
        }
        if (settings.hasKey(kPrefsSection, spacingKey)) {
            const std::string v = settings.get(kPrefsSection, spacingKey);
            if (!parseSpacing(v, level.spacing)) warn(spacingKey, v);
        }
    };
    readBool("crosshair", p.crosshair);
    readBool("grid", p.grid);
    readLevel("majorColor", "majorSpacing", p.major);
    readLevel("minorColor", "minorSpacing", p.minor);
    s_prefs = p;
    s_savedPrefs = p;
}

// Writes s_savedPrefs: only the values that differ from the defaults; the section goes when nothing differs.
void savePrefs()
{
    s_prefsDirty = false;
    SettingsFile& settings = SettingsFile::instance();
    if (!settings.persistenceEnabled()) return; // --isolated: kept in memory for the session
    settings.load();
    const ViewerPrefs d;
    const ViewerPrefs& p = s_savedPrefs;
    std::vector<SettingsEntry> entries;
    if (p.crosshair != d.crosshair) entries.emplace_back("crosshair", p.crosshair ? "1" : "0");
    if (p.grid != d.grid) entries.emplace_back("grid", p.grid ? "1" : "0");
    if (p.major.color != d.major.color) entries.emplace_back("majorColor", colorText(p.major.color));
    if (p.major.spacing != d.major.spacing) entries.emplace_back("majorSpacing", std::to_string(p.major.spacing));
    if (p.minor.color != d.minor.color) entries.emplace_back("minorColor", colorText(p.minor.color));
    if (p.minor.spacing != d.minor.spacing) entries.emplace_back("minorSpacing", std::to_string(p.minor.spacing));
    const std::vector<SettingsEntry> current = settings.section(kPrefsSection);
    if (current == entries) return;
    if (entries.empty() && !settings.hasSection(kPrefsSection)) return;
    settings.replaceSection(kPrefsSection, std::move(entries)); // an empty list removes the section
    settings.save();
}

// A preference edit from the UI (or a script with set_persist): the changed fields go into the saved copy.
void commitPrefs(const ViewerPrefs& before)
{
    const ViewerPrefs& now = s_prefs;
    bool changed = false;
    if (now.crosshair != before.crosshair) { s_savedPrefs.crosshair = now.crosshair; changed = true; }
    if (now.grid != before.grid) { s_savedPrefs.grid = now.grid; changed = true; }
    if (now.major.color != before.major.color) { s_savedPrefs.major.color = now.major.color; changed = true; }
    if (now.major.spacing != before.major.spacing) { s_savedPrefs.major.spacing = now.major.spacing; changed = true; }
    if (now.minor.color != before.minor.color) { s_savedPrefs.minor.color = now.minor.color; changed = true; }
    if (now.minor.spacing != before.minor.spacing) { s_savedPrefs.minor.spacing = now.minor.spacing; changed = true; }
    if (changed) s_prefsDirty = true;
}

ImU32 levelColor(const GridLevel& level)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(level.color[0], level.color[1], level.color[2], level.color[3]));
}

bool levelVisible(const GridLevel& level, float zoom)
{
    return static_cast<float>(level.spacing) * zoom >= kMinGridScreenSpacing;
}

// One level's lines at image x = k * spacing (0 <= k * spacing < width; same for y), 1 screen pixel wide at the
// screen pixel round(imageMin + k * spacing * zoom), only those inside the visible part of the image. `skip` (> 0)
// leaves out the lines that are also a multiple of it (the major lines over the minor level).
int drawGridLevel(ImDrawList* dl, const GridLevel& level, int skip, uint32_t width, uint32_t height, float zoom,
                  ImVec2 imageMin, ImVec2 visMin, ImVec2 visMax)
{
    if (level.spacing <= 0 || level.color[3] <= 0.0f || !levelVisible(level, zoom)) return 0;
    const ImU32 color = levelColor(level);
    const double step = static_cast<double>(level.spacing) * zoom;
    int lines = 0;
    auto axis = [&](uint32_t size, float origin, float lo, float hi, bool vertical) {
        const double kFirst = std::max(0.0, std::ceil((lo - origin - 1.0) / step));
        for (double k = kFirst;; k += 1.0) {
            const double imagePos = k * level.spacing;
            if (imagePos >= size) break;
            // The first screen column (row) of image pixel k * spacing as nearest sampling shows it.
            const float s = static_cast<float>(std::floor(static_cast<double>(origin) + k * step + 0.5));
            if (s >= hi) break;
            if (s + 1.0f <= lo) continue;
            if (skip > 0 && static_cast<long long>(imagePos) % skip == 0) continue;
            if (vertical) {
                dl->AddRectFilled(ImVec2(s, visMin.y), ImVec2(s + 1.0f, visMax.y), color);
            } else {
                dl->AddRectFilled(ImVec2(visMin.x, s), ImVec2(visMax.x, s + 1.0f), color);
            }
            ++lines;
        }
    };
    axis(width, imageMin.x, visMin.x, visMax.x, true);
    axis(height, imageMin.y, visMin.y, visMax.y, false);
    return lines;
}

void drawGrid(ViewerWindow& w, ImDrawList* dl, uint32_t width, uint32_t height, ImVec2 imageMin, ImVec2 visMin, ImVec2 visMax)
{
    w.gridLines[0] = w.gridLines[1] = 0;
    if (!s_prefs.grid || visMax.x <= visMin.x || visMax.y <= visMin.y) return;
    // Minor first, without the lines the major level draws; then the major lines on top.
    const bool majorShown = levelVisible(s_prefs.major, w.zoom) && s_prefs.major.color[3] > 0.0f;
    w.gridLines[1] = drawGridLevel(dl, s_prefs.minor, majorShown ? s_prefs.major.spacing : 0, width, height, w.zoom,
                                   imageMin, visMin, visMax);
    w.gridLines[0] = drawGridLevel(dl, s_prefs.major, 0, width, height, w.zoom, imageMin, visMin, visMax);
}

// Inverting blend for the crosshair: destination = 1 - destination (white source), destination alpha kept.
void invertBlendCallback(const ImDrawList*, const ImDrawCmd*)
{
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE_MINUS_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
}

// One horizontal and one vertical line through the centre of image pixel (x, y), 1 screen pixel wide, over the
// visible image rectangle, in inverted colour. ResetRenderState afterwards restores the backend's blend state.
void drawCrosshair(ViewerWindow& w, ImDrawList* dl, ImVec2 imageMin, ImVec2 visMin, ImVec2 visMax)
{
    w.crossDrawn = false;
    if (!s_prefs.crosshair || visMax.x <= visMin.x || visMax.y <= visMin.y) return;
    uint32_t x = 0, y = 0;
    if (w.crossHover) {
        x = w.crossX;
        y = w.crossY;
    } else if (w.crossPinned) {
        x = w.crossPinX;
        y = w.crossPinY;
    } else {
        return;
    }
    const float sx = std::floor(imageMin.x + (static_cast<float>(x) + 0.5f) * w.zoom);
    const float sy = std::floor(imageMin.y + (static_cast<float>(y) + 0.5f) * w.zoom);
    w.crossScreen = ImVec2(sx, sy);
    w.crossDrawn = true;
    dl->AddCallback(invertBlendCallback, nullptr);
    if (sx >= visMin.x && sx < visMax.x) dl->AddRectFilled(ImVec2(sx, visMin.y), ImVec2(sx + 1.0f, visMax.y), IM_COL32_WHITE);
    if (sy >= visMin.y && sy < visMax.y) {
        // The horizontal line leaves out the crossing pixel, which the vertical line already inverted.
        if (sx > visMin.x) dl->AddRectFilled(ImVec2(visMin.x, sy), ImVec2(std::min(sx, visMax.x), sy + 1.0f), IM_COL32_WHITE);
        if (sx + 1.0f < visMax.x) dl->AddRectFilled(ImVec2(std::max(sx + 1.0f, visMin.x), sy), ImVec2(visMax.x, sy + 1.0f), IM_COL32_WHITE);
    }
    dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
}

// Image -> grid -> UV overlay -> crosshair, over the visible part of the image (the area clip is already pushed).
void drawImageOverlays(ViewerWindow& w, ImDrawList* dl, uint32_t width, uint32_t height, ImVec2 imageMin, ImVec2 imageMax)
{
    const ImVec2 visMin(std::max(imageMin.x, w.areaMin.x), std::max(imageMin.y, w.areaMin.y));
    const ImVec2 visMax(std::min(imageMax.x, w.areaMin.x + w.areaSize.x), std::min(imageMax.y, w.areaMin.y + w.areaSize.y));
    drawGrid(w, dl, width, height, imageMin, visMin, visMax);
    drawUvOverlay(w, dl, imageMin, imageMax);
    drawCrosshair(w, dl, imageMin, visMin, visMax);
}

// A small icon toggle in the status bar, highlighted when on.
bool iconToggle(const char* id, const char* icon, bool on, const char* tooltip)
{
    if (on) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    }
    ImGui::PushID(id);
    const bool clicked = ImGui::Button(icon, ImVec2(ImGui::GetFrameHeight(), 0.0f));
    ImGui::PopID();
    if (on) ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

void gridLevelEditor(const char* label, const char* probe, GridLevel& level, float zoom)
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::ColorEdit4("Colour", level.color.data(), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
    UiProbe::item(probe, 0);
    ImGui::SetNextItemWidth(220.0f);
    int spacing = level.spacing;
    if (ImGui::InputInt("Spacing (px)", &spacing, 1, 64)) level.spacing = std::clamp(spacing, kMinGridSpacing, kMaxGridSpacing);
    if (!levelVisible(level, zoom)) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Hidden at this zoom: %.1f px on screen (below %.0f px)",
                           level.spacing * zoom, kMinGridScreenSpacing);
    }
    ImGui::PopID();
}

// Status bar: crosshair toggle, grid toggle and the grid settings popup.
void drawOverlayButtons(ViewerWindow& w)
{
    const ViewerPrefs before = s_prefs;
    ImGui::SameLine(0.0f, 12.0f);
    if (iconToggle("crosshair", ICON_MDI_CROSSHAIRS, s_prefs.crosshair,
                   s_prefs.crosshair ? "Crosshair: on (lines through the hovered pixel)" : "Crosshair: off")) {
        s_prefs.crosshair = !s_prefs.crosshair;
    }
    UiProbe::item("imageViewer.crosshair", w.imageIndex);
    ImGui::SameLine(0.0f, 2.0f);
    if (iconToggle("grid", ICON_MDI_GRID, s_prefs.grid, s_prefs.grid ? "Pixel grid: on" : "Pixel grid: off")) {
        s_prefs.grid = !s_prefs.grid;
    }
    UiProbe::item("imageViewer.grid", w.imageIndex);
    ImGui::SameLine(0.0f, 0.0f);
    const ImVec2 cogPos = ImGui::GetCursorScreenPos();
    if (ImGui::Button(ICON_MDI_CHEVRON_DOWN "##gridSettings", ImVec2(ImGui::GetFrameHeight() * 0.75f, 0.0f))) {
        ImGui::OpenPopup("##gridSettingsPopup");
    }
    UiProbe::item("imageViewer.gridSettings", w.imageIndex);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid settings");
    // Above the button (the status bar is at the bottom of the window).
    ImGui::SetNextWindowPos(ImVec2(cogPos.x, cogPos.y - ImGui::GetStyle().ItemSpacing.y), ImGuiCond_Appearing, ImVec2(0.0f, 1.0f));
    if (ImGui::BeginPopup("##gridSettingsPopup")) {
        UiProbe::mark("imageViewer.gridPopup", w.imageIndex);
        const ViewerPrefs d;
        gridLevelEditor("Major grid", "imageViewer.grid.majorColor", s_prefs.major, w.zoom);
        ImGui::Separator();
        gridLevelEditor("Minor grid", "imageViewer.grid.minorColor", s_prefs.minor, w.zoom);
        ImGui::Separator();
        if (ImGui::Button("Reset to defaults")) {
            s_prefs.major = d.major;
            s_prefs.minor = d.minor;
        }
        UiProbe::item("imageViewer.grid.reset", w.imageIndex);
        ImGui::TextDisabled("Spacing in image pixels; lines at multiples of it.");
        ImGui::EndPopup();
    }
    commitPrefs(before);
    // Saved when no control is being dragged or typed in (not every frame of a colour drag).
    if (s_prefsDirty && !ImGui::IsAnyItemActive()) savePrefs();
}

nlohmann::json levelJson(const GridLevel& level, float zoom, int lines)
{
    return { { "spacing", level.spacing },
             { "color", level.color },
             { "screenSpacing", level.spacing * zoom },
             { "shown", levelVisible(level, zoom) && level.color[3] > 0.0f },
             { "lines", lines } };
}

nlohmann::json overlayStateJson(const ViewerWindow& w)
{
    using json = nlohmann::json;
    json cross = { { "on", s_prefs.crosshair }, { "drawn", w.crossDrawn },
                   { "source", w.crossHover ? "mouse" : (w.crossPinned ? "script" : "none") } };
    if (w.crossHover) cross["pixel"] = { w.crossX, w.crossY };
    else if (w.crossPinned) cross["pixel"] = { w.crossPinX, w.crossPinY };
    if (w.crossDrawn) cross["screen"] = { w.crossScreen.x, w.crossScreen.y };
    json grid = { { "on", s_prefs.grid },
                  { "major", levelJson(s_prefs.major, w.zoom, w.gridLines[0]) },
                  { "minor", levelJson(s_prefs.minor, w.zoom, w.gridLines[1]) },
                  { "minScreenSpacing", kMinGridScreenSpacing } };
    return { { "crosshair", cross }, { "grid", grid } };
}

void drawStatusBar(ViewerWindow& w)
{
    PixelSet& px = w.active();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float rowEndX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

    char zoomText[32];
    if (w.fit) {
        std::snprintf(zoomText, sizeof(zoomText), "Fit Window");
    } else if (w.zoom < 0.1f) {
        std::snprintf(zoomText, sizeof(zoomText), "%.1f%%", w.zoom * 100.0f);
    } else {
        std::snprintf(zoomText, sizeof(zoomText), "%.0f%%", w.zoom * 100.0f);
    }
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Fit Window").x + ImGui::GetFrameHeight() + style.FramePadding.x * 2.0f);
    if (ImGui::BeginCombo("##zoom", zoomText)) {
        static const float presets[] = { 2.0f, 1.0f, 0.75f, 0.5f, 0.25f };
        for (float preset : presets) {
            char label[16];
            std::snprintf(label, sizeof(label), "%.0f%%", preset * 100.0f);
            if (ImGui::Selectable(label, !w.fit && std::abs(w.zoom - preset) < 1e-4f)) setZoom(w, preset);
        }
        if (ImGui::Selectable("Fit Window", w.fit)) w.fit = true;
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Zoom. The mouse wheel zooms around the cursor; drag the image to pan.");

    for (uint32_t c = 0; c < px.channels; ++c) {
        ImGui::SameLine(0.0f, c == 0 ? 12.0f : 2.0f);
        const char label[2] = { px.label(c), 0 };
        ImGui::PushID(static_cast<int>(c));
        const bool on = w.on[c];
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, channelColor(px, c, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, channelColor(px, c, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        }
        if (ImGui::Button(label, ImVec2(ImGui::GetFrameHeight(), 0.0f))) w.on[c] = !w.on[c];
        if (on) ImGui::PopStyleColor(3);
        UiProbe::item("imageViewer.channel", static_cast<long long>(c));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s the %c channel. One channel alone is shown as grayscale; without A the image is opaque.",
                              on ? "Hide" : "Show", px.label(c));
        }
        ImGui::PopID();
    }

    ImGui::SameLine(0.0f, 12.0f);
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Checkerboard").x + ImGui::GetFrameHeight() + style.FramePadding.x * 2.0f);
    ImGui::Combo("##background", &w.background, kBackgroundNames, IM_ARRAYSIZE(kBackgroundNames));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Background behind transparent pixels");

    if (w.hasDecoded) {
        ImGui::SameLine(0.0f, 12.0f);
        bool raw = w.raw;
        if (ImGui::Checkbox("Raw", &raw)) {
            w.raw = raw;
            resetView(w);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Off: %s.\nOn: the values the file stores (signed X/Y).", w.decodedNote.c_str());
        }
    }
    drawOverlayButtons(w);

    const PixelSet& shown = w.active();
    ImGui::SameLine(0.0f, 16.0f);
    ImGui::AlignTextToFramePadding();
    if (w.haveReadout && w.readX < shown.width && w.readY < shown.height) {
        ImGui::Text("x %u  y %u", w.readX, w.readY);
    } else {
        ImGui::TextDisabled("x -  y -");
    }

    // While loading: a spinner and the phase (the preview is drawn meanwhile).
    const std::string phase = loadPhase(w);
    if (phase != "ready") {
        static const char* spinner[4] = { "|", "/", "-", "\\" };
        const char* glyph = spinner[static_cast<int>(ImGui::GetTime() * 8.0) & 3];
        ImGui::SameLine(0.0f, 16.0f);
        std::string label;
        if (phase == "decoding" || phase == "reading") {
            label = "Loading full resolution...";
        } else if (phase == "uploading") {
            label = "Uploading " + std::to_string(static_cast<int>(100.0 * shown.uploadedRows / std::max(shown.height, 1u))) + "%...";
        } else if (phase == "histogram") {
            label = "Computing histogram...";
        } else {
            label = "Building the UV layout...";
        }
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s %s", glyph, label.c_str());
        UiProbe::item("imageViewer.loading", w.imageIndex);
    }

    // Image facts, right-aligned when they fit (else they follow the pixel and clip at the window edge).
    std::string facts = std::to_string(shown.width) + " x " + std::to_string(shown.height) + " px  ";
    for (uint32_t c = 0; c < shown.channels; ++c) facts += shown.label(c);
    facts += "  ";
    if (!w.pixelsReady) {
        facts += "preview";
    } else if (shown.type == SampleType::F32) {
        facts += shown.storedBits == 16 ? "16-bit float" : "32-bit float";
    } else {
        facts += std::to_string(shown.type == SampleType::U16 ? 16 : 8) + "-bit" + (shown.type == SampleType::S8 ? " signed" : "");
    }
    if (!w.source.empty()) facts += "  " + w.source;
    if (w.hasDecoded && !w.raw) facts += "  (decoded normal map)";
    const bool warn = !w.warnings.empty();
    const float markerWidth = warn ? ImGui::CalcTextSize("(!) ").x : 0.0f;
    const float factsWidth = markerWidth + ImGui::CalcTextSize(facts.c_str()).x;
    ImGui::SameLine(0.0f, 16.0f);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    if (cursor.x + factsWidth <= rowEndX) ImGui::SetCursorScreenPos(ImVec2(rowEndX - factsWidth, cursor.y));
    ImGui::BeginGroup();
    if (warn) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "(!)");
        ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
    }
    ImGui::TextDisabled("%s", facts.c_str());
    ImGui::EndGroup();
    if (ImGui::IsItemHovered()) {
        std::string tip = facts;
        if (w.hasDecoded) tip += "\n" + (w.raw ? std::string("Raw: the stored values") : w.decodedNote);
        for (const auto& warning : w.warnings) tip += "\nNote: " + warning;
        ImGui::SetTooltip("%s", tip.c_str());
    }
}

// ---------------------------------------------------------------------------------------------
// Image area
// ---------------------------------------------------------------------------------------------

void drawImageArea(ViewerWindow& w, ImVec2 size)
{
    PixelSet& px = w.active();
    const ImVec2 areaMin = ImGui::GetCursorScreenPos();
    const ImVec2 areaSize(std::max(size.x, 1.0f), std::max(size.y, 1.0f));
    w.areaMin = areaMin;
    w.areaSize = areaSize;
    const ImVec2 areaMax(areaMin.x + areaSize.x, areaMin.y + areaSize.y);

    if (w.fit) {
        w.zoom = std::clamp(std::min(areaSize.x / static_cast<float>(px.width), areaSize.y / static_cast<float>(px.height)),
                            kMinZoom, kMaxZoom);
        w.centerX = px.width * 0.5;
        w.centerY = px.height * 0.5;
    }
    clampView(w);

    ImGui::InvisibleButton("##imageArea", areaSize, ImGuiButtonFlags_MouseButtonLeft);
    UiProbe::item("imageViewer.area", w.imageIndex);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 areaCenter(areaMin.x + areaSize.x * 0.5f, areaMin.y + areaSize.y * 0.5f);

    if (hovered && io.MouseWheel != 0.0f) {
        // Zoom around the cursor: the image point under it stays under it.
        const double pointX = w.centerX + (io.MousePos.x - areaCenter.x) / w.zoom;
        const double pointY = w.centerY + (io.MousePos.y - areaCenter.y) / w.zoom;
        w.fit = false;
        w.zoom = std::clamp(w.zoom * std::pow(kWheelStep, io.MouseWheel), kMinZoom, kMaxZoom);
        w.centerX = pointX - (io.MousePos.x - areaCenter.x) / w.zoom;
        w.centerY = pointY - (io.MousePos.y - areaCenter.y) / w.zoom;
        clampView(w);
    }
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        w.centerX -= io.MouseDelta.x / w.zoom;
        w.centerY -= io.MouseDelta.y / w.zoom;
        clampView(w);
    }

    const float imageW = static_cast<float>(px.width) * w.zoom;
    const float imageH = static_cast<float>(px.height) * w.zoom;
    ImVec2 imageMin(static_cast<float>(areaCenter.x - w.centerX * w.zoom), static_cast<float>(areaCenter.y - w.centerY * w.zoom));
    imageMin = ImVec2(std::floor(imageMin.x + 0.5f), std::floor(imageMin.y + 0.5f));
    const ImVec2 imageMax(imageMin.x + imageW, imageMin.y + imageH);
    w.imageMin = imageMin;
    w.imageMax = imageMax;

    w.crossHover = false;
    if (hovered && io.MousePos.x >= imageMin.x && io.MousePos.y >= imageMin.y && io.MousePos.x < imageMax.x
        && io.MousePos.y < imageMax.y) {
        const double ix = std::floor((io.MousePos.x - imageMin.x) / w.zoom);
        const double iy = std::floor((io.MousePos.y - imageMin.y) / w.zoom);
        if (ix >= 0.0 && iy >= 0.0 && ix < px.width && iy < px.height) {
            w.crossHover = true; // also on the preview: the size comes from the header
            w.crossX = static_cast<uint32_t>(ix);
            w.crossY = static_cast<uint32_t>(iy);
        }
        if (w.pixelsReady && ix >= 0.0 && iy >= 0.0 && ix < px.width && iy < px.height) {
            w.haveReadout = true;
            w.readX = static_cast<uint32_t>(ix);
            w.readY = static_cast<uint32_t>(iy);
        }
    }
    if (active && (imageW > areaSize.x || imageH > areaSize.y)) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(areaMin, areaMax, IM_COL32(0, 0, 0, 255));
    if (!px.uploadError.empty()) {
        const std::string text = "The image cannot be shown on the GPU: " + px.uploadError;
        dl->AddText(ImVec2(areaMin.x + 12.0f, areaMin.y + 12.0f), IM_COL32(230, 120, 120, 255), text.c_str());
        return;
    }
    if (!px.textureComplete) {
        // The preview (an existing small texture at the image's aspect) until the full texture is complete.
        dl->PushClipRect(areaMin, areaMax, true);
        if (w.previewTexture != 0 && glIsTexture(w.previewTexture) == GL_TRUE) {
            dl->AddImage(static_cast<ImTextureID>(w.previewTexture), imageMin, imageMax);
        } else {
            dl->AddRectFilled(imageMin, imageMax, IM_COL32(40, 40, 44, 255));
        }
        drawImageOverlays(w, dl, px.width, px.height, imageMin, imageMax);
        const char* label = "Preview - loading full resolution";
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const ImVec2 labelMin(areaMin.x + 8.0f, areaMin.y + 8.0f);
        dl->AddRectFilled(labelMin, ImVec2(labelMin.x + textSize.x + 12.0f, labelMin.y + textSize.y + 8.0f), IM_COL32(0, 0, 0, 170), 3.0f);
        dl->AddText(ImVec2(labelMin.x + 6.0f, labelMin.y + 4.0f), IM_COL32(255, 210, 90, 255), label);
        dl->PopClipRect();
        return;
    }
    dl->PushClipRect(areaMin, areaMax, true);
    if (ensureProgram()) {
        DrawParams params{};
        params.sampler = w.zoom >= 0.999f ? s_samplerNearest : s_samplerLinear;
        params.channelCount = static_cast<GLint>(px.channels);
        params.alphaIndex = px.alphaIndex();
        params.luminance = px.luminance ? 1 : 0;
        for (uint32_t c = 0; c < 4; ++c) params.on[c] = c < px.channels && w.on[c] ? 1.0f : 0.0f;
        switch (px.type) {
        case SampleType::U8: params.native[0] = 255.0f; params.native[1] = 0.0f; break;
        case SampleType::S8: params.native[0] = 255.0f; params.native[1] = -128.0f; break;
        case SampleType::U16: params.native[0] = 65535.0f; params.native[1] = 0.0f; break;
        default: params.native[0] = 1.0f; params.native[1] = 0.0f; break;
        }
        params.levels[0] = static_cast<float>(w.low);
        params.levels[1] = static_cast<float>(w.high);
        params.background = w.background;
        params.checker = 8.0f * std::max(io.DisplayFramebufferScale.x, 1.0f);
        dl->AddCallback(drawCallback, &params, sizeof(params));
        dl->AddImage(static_cast<ImTextureID>(px.texture), imageMin, imageMax);
        dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
    } else {
        // No program: the plain texture (no channel mask or levels).
        dl->AddImage(static_cast<ImTextureID>(px.texture), imageMin, imageMax);
    }
    drawImageOverlays(w, dl, px.width, px.height, imageMin, imageMax);
    dl->PopClipRect();
}

void renderWindow(ViewerWindow& w, bool& open)
{
    const ImGuiViewport* main = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(1000.0f, 720.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(main->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    if (w.focusRequest) {
        ImGui::SetNextWindowFocus();
        w.focusRequest = false;
    }
    // image_viewer window_pos / window_size (scripts): relative to the main window; with multi-viewport a window
    // placed outside the main window becomes its own OS window.
    if (w.pendingPos) {
        ImGui::SetNextWindowPos(ImVec2(main->Pos.x + w.requestedPos.x, main->Pos.y + w.requestedPos.y), ImGuiCond_Always);
        w.pendingPos = false;
    }
    if (w.pendingSize) {
        ImGui::SetNextWindowSize(w.requestedSize, ImGuiCond_Always);
        w.pendingSize = false;
    }
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin(w.title.c_str(), &open, flags)) {
        UiProbe::mark("imageViewer.window", w.imageIndex);
        if (!w.error.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.45f, 1.0f), "%s", w.error.c_str());
        } else {
            PixelSet& px = w.active();
            if (w.pixelsReady) uploadStep(px, s_uploadBudget, &w.times);
            if (w.uv) updateUvBuild(*w.uv);
            // Top: picker | histogram (4 frame-height rows); then the image; one status row at the bottom.
            const ImGuiStyle& style = ImGui::GetStyle();
            const float topHeight = ImGui::GetFrameHeight() * 4.0f;
            drawTopBar(w, topHeight);
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            // A UV overlay adds a second status row (its controls do not fit beside the image facts).
            const int statusRows = w.uv ? 2 : 1;
            const float statusHeight = ImGui::GetFrameHeight() * statusRows + style.ItemSpacing.y * (statusRows - 1);
            drawImageArea(w, ImVec2(avail.x, avail.y - statusHeight - style.ItemSpacing.y));
            drawStatusBar(w);
            if (w.uv) drawUvStatusRow(w);
        }
        if (const ImGuiWindow* current = ImGui::GetCurrentWindow()) {
            w.windowMin = current->Pos;
            w.windowSize = current->Size;
            w.detached = current->Viewport != nullptr && current->Viewport != ImGui::GetMainViewport();
        }
    }
    ImGui::End();
}

nlohmann::json stateJson(const ViewerWindow& w)
{
    using json = nlohmann::json;
    const PixelSet& px = w.active();
    json out;
    out["key"] = w.key;
    out["title"] = w.title;
    out["image"] = w.imageIndex;
    out["windows"] = s_windows.size();
    const std::string phase = loadPhase(w);
    out["loading"] = phase;
    out["ready"] = phase == "ready";
    out["pixelsReady"] = w.pixelsReady;
    out["histogramReady"] = w.statsReady;
    out["textureComplete"] = w.active().textureComplete;
    out["preview"] = w.previewTexture != 0;
    out["timings"] = {
        { "copyMs", w.times.copyMs }, { "decodeMs", w.times.decodeMs }, { "convertMs", w.times.convertMs },
        { "variantMs", w.times.variantMs }, { "histogramMs", w.times.histogramMs }, { "uploadMs", w.times.uploadMs },
        { "uploadFrames", w.times.uploadFrames }, { "mipmapMs", w.times.mipmapMs }, { "sharpMs", w.times.sharpMs },
        { "allocMs", w.times.allocMs },
        { "readyMs", w.times.readyMs }, { "maxViewerMs", w.times.maxViewerMs }, { "maxFrameMs", w.times.maxFrameMs },
    };
    if (!w.error.empty()) {
        out["error"] = w.error;
        return out;
    }
    out["width"] = px.width;
    out["height"] = px.height;
    json labels = json::array();
    json enabled = json::array();
    json dataRange = json::object();
    for (uint32_t c = 0; c < px.channels; ++c) {
        const std::string label(1, px.label(c));
        labels.push_back(label);
        if (w.on[c]) enabled.push_back(label);
        dataRange[label] = { px.dataMin[c], px.dataMax[c] };
    }
    out["channels"] = labels;
    out["shown"] = enabled;
    out["type"] = px.typeName();
    out["storedBits"] = px.storedBits;
    out["source"] = w.source;
    out["decodedView"] = w.hasDecoded;
    out["raw"] = w.raw;
    if (w.hasDecoded) out["decodedNote"] = w.decodedNote;
    out["signedSquare"] = w.signedSquare;
    out["fit"] = w.fit;
    out["zoomPercent"] = w.zoom * 100.0f;
    out["center"] = { w.centerX, w.centerY };
    out["levels"] = { w.low, w.high };
    out["fullRange"] = { px.rangeMin(), px.rangeMax() };
    out["dataRange"] = dataRange;
    out["background"] = kBackgroundKeys[std::clamp(w.background, 0, 3)];
    out["warnings"] = w.warnings;
    out["area"] = { w.areaMin.x, w.areaMin.y, w.areaMin.x + w.areaSize.x, w.areaMin.y + w.areaSize.y };
    out["textureUploaded"] = px.texture != 0;
    out["window"] = { w.windowMin.x, w.windowMin.y, w.windowMin.x + w.windowSize.x, w.windowMin.y + w.windowSize.y };
    out["detached"] = w.detached;
    out["splitFraction"] = s_splitFraction;
    out["imageRect"] = { w.imageMin.x, w.imageMin.y, w.imageMax.x, w.imageMax.y };
    out.update(overlayStateJson(w));
    if (w.uv) out["uvLayout"] = uvStateJson(*w.uv);
    if (!px.uploadError.empty()) out["uploadError"] = px.uploadError;
    if (w.haveReadout && w.readX < px.width && w.readY < px.height) {
        json values = json::array();
        for (uint32_t c = 0; c < px.channels; ++c) values.push_back(px.value(w.readX, w.readY, c));
        out["readout"] = { { "x", w.readX }, { "y", w.readY }, { "values", values } };
    }
    return out;
}

ViewerWindow* openWindow(const ux3d::gltf::Gltf& model, const ux3d::gltf::Image* image, bool signedSquareNormal,
                         unsigned int previewTexture = 0)
{
    if (image == nullptr || findImage(model, image) == nullptr) return nullptr;
    const std::string key = imageKey(image);
    auto it = s_windows.find(key);
    if (it == s_windows.end()) {
        auto window = std::make_unique<ViewerWindow>();
        window->key = key;
        it = s_windows.emplace(key, std::move(window)).first;
    }
    ViewerWindow& w = *it->second;
    w.focusRequest = true;
    const bool fresh = w.image == nullptr;
    if (fresh || w.image != image || w.signedSquare != signedSquareNormal || w.encodedMapID != image->encodedMapID) {
        w.image = image;
        w.imageIndex = image->m_ID;
        w.signedSquare = signedSquareNormal;
        w.displayName = getImageDisplayName(image);
        w.title = w.displayName + " - Image Viewer###ImageViewer:" + key;
        w.previewTexture = previewTexture;
        startLoad(w);
        if (fresh) {
            w.raw = false;
            w.fit = true;
            w.background = 0;
            w.haveReadout = false;
        }
        resetView(w);
    } else if (previewTexture != 0 && !w.active().textureComplete) {
        w.previewTexture = previewTexture;
    }
    return &w;
}

} // namespace

void ImageViewer::open(const ux3d::gltf::Gltf& model, const ux3d::gltf::Image* image, bool signedSquareNormal,
                       unsigned int previewTexture)
{
    openWindow(model, image, signedSquareNormal, previewTexture);
}

ImageViewer::UvLayoutRequest ImageViewer::uvLayoutRequest(const ux3d::gltf::Material* material, const char* slot,
                                                          const ux3d::gltf::TextureInfo& info)
{
    UvLayoutRequest request;
    request.material = material;
    request.slot = slot != nullptr ? slot : "";
    request.texCoord = info.getEffectiveTexCoord(); // texCoord, or the KHR_texture_transform override
    request.hasTransform = info.hasTextureTransform();
    if (request.hasTransform) {
        const auto m = info.calculateTransformMatrix(); // the renderer's u_<slot>UVTransform
        for (int column = 0; column < 3; ++column) {
            for (int row = 0; row < 3; ++row) request.transform[static_cast<size_t>(column * 3 + row)] = m[column][row];
        }
    }
    return request;
}

namespace {

ViewerWindow* attachUvLayout(const ux3d::gltf::Gltf& model, const ux3d::renderer::GltfRenderLayer* layer,
                             const ux3d::gltf::Image* image, bool signedSquareNormal,
                             const ImageViewer::UvLayoutRequest& request, unsigned int previewTexture)
{
    if (request.material == nullptr || !materialInModel(model, request.material)) return nullptr;
    ViewerWindow* w = openWindow(model, image, signedSquareNormal, previewTexture);
    if (w == nullptr) return nullptr;
    const bool same = w->uv && w->uv->material == request.material && w->uv->texCoord == request.texCoord
        && w->uv->hasTransform == request.hasTransform && w->uv->transform == request.transform;
    if (same) {
        w->uv->slot = request.slot;
        w->uv->show = true;
        return w;
    }
    dropOverlay(*w);
    auto o = std::make_unique<UvOverlay>();
    o->material = request.material;
    o->materialIndex = request.material->m_ID;
    o->materialName = request.material->hasName() ? std::string(request.material->getName().c_str())
                                                  : "Material " + std::to_string(request.material->m_ID);
    o->slot = request.slot;
    o->texCoord = request.texCoord;
    o->hasTransform = request.hasTransform;
    o->transform = request.transform;
    gatherUvTriangles(*o, model, layer);
    updateUvBuild(*o); // the edges are built on a worker
    core::logInfo("[Image Viewer] UV layout of '%s' TEXCOORD_%u: %u primitives, %zu triangles (read in %.1f ms)",
                  o->materialName.c_str(), o->texCoord, o->primitives, o->triangleCount, o->gatherMs);
    w->uv = std::move(o);
    return w;
}

} // namespace

void ImageViewer::openUvLayout(const ux3d::gltf::Gltf& model, const ux3d::renderer::GltfRenderLayer* layer,
                               const ux3d::gltf::Image* image, bool signedSquareNormal, const UvLayoutRequest& request,
                               unsigned int previewTexture)
{
    attachUvLayout(model, layer, image, signedSquareNormal, request, previewTexture);
}

bool ImageViewer::uvLayoutScript(const ux3d::gltf::Gltf& model, ux3d::renderer::GltfRenderLayer* layer,
                                 const nlohmann::json& args, nlohmann::json& result, std::string& errorCode,
                                 std::string& errorMessage)
{
    auto fail = [&](const char* code, const std::string& message) {
        errorCode = code;
        errorMessage = message;
        return false;
    };
    const long long index = args.value("material", -1LL);
    const ux3d::gltf::Material* material = nullptr;
    for (const auto& candidate : model.getMaterials()) {
        if (static_cast<long long>(candidate.m_ID) == index) material = &candidate;
    }
    if (material == nullptr) return fail("material_not_found", "no material with index " + std::to_string(index));
    std::string slot = args.contains("slot") && args["slot"].is_string() ? lowerText(args["slot"].get<std::string>()) : "basecolor";
    if (slot.size() > 7 && slot.compare(slot.size() - 7, 7, "texture") == 0) slot.resize(slot.size() - 7);
    const std::optional<ux3d::gltf::TextureInfo>* info = nullptr;
    const char* label = nullptr;
    bool normalSlot = false;
    const auto& mr = material->getPbrMetallicRoughness();
    if (slot == "basecolor" && mr.has_value()) {
        info = &mr->getBaseColorTextureInfo();
        label = "Base Color Texture";
    } else if (slot == "metallicroughness" && mr.has_value()) {
        info = &mr->getMetallicRoughnessTextureInfo();
        label = "Metallic-Roughness Texture";
    } else if (slot == "normal") {
        info = &material->getNormalTextureInfo();
        label = "Normal Texture";
        normalSlot = true;
    } else if (slot == "occlusion") {
        info = &material->getOcclusionTextureInfo();
        label = "Occlusion Texture";
    } else if (slot == "emissive") {
        info = &material->getEmissiveTextureInfo();
        label = "Emissive Texture";
    } else if (slot != "basecolor" && slot != "metallicroughness") {
        return fail("bad_arguments", "slot: baseColor, metallicRoughness, normal, occlusion or emissive");
    }
    if (info == nullptr || !info->has_value() || (*info)->getTexture() == nullptr) {
        return fail("no_texture", "the slot has no texture");
    }
    const ux3d::gltf::Texture* texture = (*info)->getTexture();
    const ux3d::gltf::Image* image = ux3d::renderer::GltfRenderLayer::pickImageForTexture(
        texture, model.isExtensionDeclared("MSFT_texture_dds"));
    if (image == nullptr) image = texture->getSource();
    if (image == nullptr) return fail("no_texture", "the slot's texture has no source image");
    const bool signedSquare = normalSlot && model.isExtensionDeclared("ASOBO_asset_optimized");
    const unsigned int preview = ElementInspector::getImagePreviewTextureId(image, layer, signedSquare);
    ViewerWindow* w = attachUvLayout(model, layer, image, signedSquare, uvLayoutRequest(material, label, **info), preview);
    if (w == nullptr || !w->uv) return fail("image_not_found", "the image is not part of the model");

    auto toggle = [&](const char* name, bool& value) -> bool {
        if (!args.contains(name) || args[name].is_null()) return true;
        const auto& v = args[name];
        if (v.is_boolean()) {
            value = v.get<bool>();
            return true;
        }
        const std::string t = v.is_string() ? lowerText(v.get<std::string>()) : v.dump();
        if (t == "-" || t.empty()) return true;
        if (t == "on" || t == "true" || t == "1") value = true;
        else if (t == "off" || t == "false" || t == "0") value = false;
        else return false;
        return true;
    };
    UvOverlay& o = *w->uv;
    if (!toggle("wrap", o.wrap)) return fail("bad_arguments", "wrap: on, off or -");
    if (!toggle("fill", o.fill)) return fail("bad_arguments", "fill: on, off or -");
    bool transform = o.applyTransform;
    if (!toggle("transform", transform)) return fail("bad_arguments", "transform: on, off or -");
    if (o.hasTransform) o.applyTransform = transform; // without a transform there is nothing to switch
    if (!toggle("show", o.show)) return fail("bad_arguments", "show: on, off or -");
    if (args.contains("opacity") && args["opacity"].is_number()) o.opacity = std::clamp(args["opacity"].get<float>(), 0.05f, 1.0f);
    updateUvBuild(o); // starts the build on a worker; the counts arrive with it (image_viewer_wait)
    result = stateJson(*w);
    return true;
}

void ImageViewer::render(const ux3d::gltf::Gltf& model, const std::function<bool(const char* windowName)>& areaShows)
{
    const auto frameStart = Clock::now();
    const float frameMs = ImGui::GetIO().DeltaTime * 1000.0f;
    ensurePrefsLoaded();
    s_uploadBudget = kUploadBytesPerFrame; // row bands of every viewer together
    reapRetired(false);
    for (auto it = s_windows.begin(); it != s_windows.end();) {
        ViewerWindow& w = *it->second;
        // The model changed under the window: re-resolve by key, or close (never keep a dangling pointer).
        if (w.needsReload) {
            w.needsReload = false;
            dropOverlay(w);        // the model was read again: its materials are new objects
            w.previewTexture = 0;  // the Inspector's preview cache was cleared
            w.image = findImageByKey(model, w.key);
            if (w.image != nullptr) {
                w.imageIndex = w.image->m_ID;
                startLoad(w);
                resetView(w);
            }
        } else if (w.image != nullptr && findImage(model, w.image) == nullptr) {
            w.image = nullptr;
        }
        if (w.image == nullptr) {
            releaseWindow(w);
            it = s_windows.erase(it);
            continue;
        }
        if (w.uv && !materialInModel(model, w.uv->material)) dropOverlay(w); // the material disappeared
        if (w.encodedMapID != w.image->encodedMapID) {
            startLoad(w);
            resetView(w);
        }
        continueLoad(w);
        pollLoad(w);
        // A docked viewer follows Maximize Area like the other panels; a floating one stays visible.
        const ImGuiWindow* existing = ImGui::FindWindowByName(w.title.c_str());
        if (existing != nullptr && existing->DockNode != nullptr && areaShows && !areaShows(w.title.c_str())) {
            ++it;
            continue;
        }
        bool open = true;
        renderWindow(w, open);
        if (!open) {
            releaseWindow(w);
            it = s_windows.erase(it);
            continue;
        }
        ++it;
    }
    // Load timings: the viewer's main-thread time this frame and the frame time, while a window is loading; the
    // summary is logged once when the window is ready.
    const double viewerMs = msBetween(frameStart, Clock::now());
    for (auto& entry : s_windows) {
        ViewerWindow& w = *entry.second;
        if (w.uv && w.uv->job) {
            w.uv->buildMaxViewerMs = std::max(w.uv->buildMaxViewerMs, viewerMs);
            w.uv->buildMaxFrameMs = std::max(w.uv->buildMaxFrameMs, static_cast<double>(frameMs));
        }
        if (w.readyLogged) continue;
        w.times.maxViewerMs = std::max(w.times.maxViewerMs, viewerMs);
        w.times.maxFrameMs = std::max(w.times.maxFrameMs, static_cast<double>(frameMs));
        if (frameMs > 34.0f || viewerMs > 15.0) {
            core::logInfo("[Image Viewer frame] %s: frame %.1f ms, viewer %.1f ms, phase %s (%.0f ms since open)",
                          w.displayName.c_str(), frameMs, viewerMs, loadPhase(w).c_str(), msBetween(w.loadStart, Clock::now()));
        }
        if (w.times.sharpMs < 0.0 && w.active().textureComplete) w.times.sharpMs = msBetween(w.loadStart, Clock::now());
        if (loadPhase(w) == "ready") {
            w.readyLogged = true;
            w.times.readyMs = msBetween(w.loadStart, Clock::now());
            const LoadTimes& t = w.times;
            core::logInfo("[Image Viewer] %s ready in %.0f ms (sharp at %.0f ms): copy %.1f, decode %.1f, convert %.1f, "
                          "variant %.1f, histogram+range %.1f (worker) | alloc %.1f, upload %.1f in %d frames, mipmaps %.1f | "
                          "longest viewer frame %.1f ms, longest frame %.1f ms",
                          w.displayName.c_str(), t.readyMs, t.sharpMs, t.copyMs, t.decodeMs, t.convertMs, t.variantMs,
                          t.histogramMs, t.allocMs, t.uploadMs, t.uploadFrames, t.mipmapMs, t.maxViewerMs, t.maxFrameMs);
            if (w.uv) {
                core::logInfo("[Image Viewer] UV layout: read %.1f ms (main), edges %.1f ms (worker), buffers %.1f ms (main)",
                              w.uv->gatherMs, w.uv->buildMs, w.uv->bufferUploadMs);
            }
        }
    }
}

void ImageViewer::closeAll()
{
    for (auto& entry : s_windows) releaseWindow(*entry.second);
    s_windows.clear();
}

void ImageViewer::reloadAll()
{
    for (auto& entry : s_windows) {
        entry.second->needsReload = true;
        entry.second->previewTexture = 0; // the preview cache is cleared before a reload
    }
}

void ImageViewer::shutdown()
{
    closeAll();
    reapRetired(true); // every cancelled load ends and is joined: no thread outlives the viewer
    if (s_program != 0) glDeleteProgram(s_program);
    if (s_uvProgram != 0) glDeleteProgram(s_uvProgram);
    s_uvProgram = 0;
    s_uvProgramFailed = false;
    if (s_samplerNearest != 0) glDeleteSamplers(1, &s_samplerNearest);
    if (s_samplerLinear != 0) glDeleteSamplers(1, &s_samplerLinear);
    s_program = 0;
    s_samplerNearest = 0;
    s_samplerLinear = 0;
    s_programFailed = false;
    s_imguiProgram = 0;
    s_imguiProjLoc = -1;
}

size_t ImageViewer::windowCount()
{
    return s_windows.size();
}

bool ImageViewer::waitState(const ux3d::gltf::Gltf& model, long long imageIndex, nlohmann::json& state, bool& ready,
                            std::string& errorCode, std::string& errorMessage)
{
    const ux3d::gltf::Image* image = nullptr;
    for (const auto& candidate : model.getImages()) {
        if (static_cast<long long>(candidate.m_ID) == imageIndex) image = &candidate;
    }
    const auto it = image != nullptr ? s_windows.find(imageKey(image)) : s_windows.end();
    if (it == s_windows.end()) {
        errorCode = "window_not_found";
        errorMessage = "no Image Viewer window for image " + std::to_string(imageIndex);
        return false;
    }
    ready = loadPhase(*it->second) == "ready";
    state = stateJson(*it->second);
    return true;
}

namespace {

std::string argText(const nlohmann::json& args, const char* name)
{
    if (!args.contains(name) || args[name].is_null()) return std::string();
    const nlohmann::json& v = args[name];
    if (v.is_string()) return lowerText(v.get<std::string>());
    if (v.is_boolean()) return v.get<bool>() ? "on" : "off";
    if (v.is_number()) {
        char buffer[48];
        std::snprintf(buffer, sizeof(buffer), "%.17g", v.get<double>());
        return buffer;
    }
    return v.dump();
}

bool hasViewOptions(const nlohmann::json& args)
{
    for (const char* name : { "raw", "channels", "levels", "pixel" }) {
        const std::string t = argText(args, name);
        if (!t.empty() && t != "-") return true;
    }
    return false;
}

// The image_viewer options that need the pixels (raw, channels, levels, pixel) and the ones that do not (zoom,
// background). Sent while loading, the pixel ones wait in pendingOptions and are applied when the pixels arrive.
bool applyViewOptions(ViewerWindow& window, const nlohmann::json& args, std::string& errorCode, std::string& errorMessage)
{
    using json = nlohmann::json;
    ViewerWindow* w = &window;
    auto text = [&](const char* name) { return argText(args, name); };
    auto fail = [&](const char* code, const std::string& message) {
        errorCode = code;
        errorMessage = message;
        return false;
    };
    if (!w->error.empty()) return true;
    const std::string zoom = text("zoom");
    if (!zoom.empty() && zoom != "-") {
        if (zoom == "fit") {
            w->fit = true;
        } else {
            try {
                std::string number = zoom;
                if (!number.empty() && number.back() == '%') number.pop_back();
                setZoom(*w, static_cast<float>(std::stod(number) / 100.0));
            } catch (...) {
                return fail("bad_arguments", "zoom: 'fit' or a percentage, got " + zoom);
            }
        }
    }
    const std::string background = text("background");
    if (!background.empty() && background != "-") {
        int found = -1;
        for (int i = 0; i < 4; ++i) {
            if (background == kBackgroundKeys[i] || background == lowerText(kBackgroundNames[i])
                || (i == 2 && background == "grey")) {
                found = i;
            }
        }
        if (found < 0) return fail("bad_arguments", "background: checker, white, gray or black, got " + background);
        w->background = found;
    }
    if (!w->pixelsReady) {
        if (hasViewOptions(args)) w->pendingOptions = args;
        return true;
    }
    const std::string raw = text("raw");
    if (!raw.empty() && raw != "-") {
        const bool wanted = raw == "on" || raw == "true" || raw == "1" || raw == "raw";
        if (wanted && !w->hasDecoded) return fail("bad_arguments", "raw: this image has no decoded view");
        if (wanted != w->raw) {
            w->raw = wanted;
            resetView(*w);
        }
    }
    PixelSet& px = w->active();
    const std::string channels = text("channels");
    if (!channels.empty() && channels != "-") {
        std::array<bool, 4> on{ false, false, false, false };
        if (channels == "all") {
            on = { true, true, true, true };
        } else {
            for (char c : channels) {
                bool found = false;
                for (uint32_t i = 0; i < px.channels; ++i) {
                    if (std::tolower(static_cast<unsigned char>(px.label(i))) == c) {
                        on[i] = true;
                        found = true;
                    }
                }
                if (!found) return fail("bad_arguments", std::string("channels: the image has no channel ") + c);
            }
        }
        w->on = on;
    }
    const std::string levels = text("levels");
    if (!levels.empty() && levels != "-") {
        if (levels == "full") {
            resetLevels(*w);
        } else if (levels == "data") {
            if (!w->statsReady) {
                // The data range comes with the histogram: applied again when it arrives.
                w->pendingOptions = json{ { "levels", "data" } };
            } else {
                setDataLevels(*w);
            }
        } else {
            double low = 0.0;
            double high = 0.0;
            if (std::sscanf(levels.c_str(), "%lf,%lf", &low, &high) != 2) {
                return fail("bad_arguments", "levels: full, data or low,high, got " + levels);
            }
            low = std::clamp(low, px.rangeMin(), px.rangeMax() - px.step());
            high = std::clamp(high, low + px.step(), px.rangeMax());
            w->low = low;
            w->high = high;
        }
    }
    const json* pixel = args.contains("pixel") ? &args["pixel"] : nullptr;
    double x = 0.0;
    double y = 0.0;
    bool havePixel = false;
    if (pixel != nullptr && pixel->is_array() && pixel->size() == 2 && (*pixel)[0].is_number() && (*pixel)[1].is_number()) {
        x = (*pixel)[0].get<double>();
        y = (*pixel)[1].get<double>();
        havePixel = true;
    } else if (pixel != nullptr && pixel->is_string() && pixel->get<std::string>() != "-") {
        if (std::sscanf(pixel->get<std::string>().c_str(), "%lf,%lf", &x, &y) != 2) {
            return fail("bad_arguments", "pixel: x,y or [x, y], got " + pixel->get<std::string>());
        }
        havePixel = true;
    } else if (pixel != nullptr && !pixel->is_null() && !pixel->is_string()) {
        return fail("bad_arguments", "pixel: x,y or [x, y]");
    }
    if (havePixel) {
        if (x < 0.0 || y < 0.0 || x >= px.width || y >= px.height) {
            return fail("bad_arguments", "pixel: outside the image");
        }
        w->haveReadout = true;
        w->readX = static_cast<uint32_t>(x);
        w->readY = static_cast<uint32_t>(y);
    }
    return true;
}

// crosshair, grid, major / minor (spacing), major_color / minor_color, grid_defaults, cursor. They are preferences:
// kept for the session, and saved to settings.ini only when the caller may persist (set_persist(true)).
bool applyOverlayOptions(ViewerWindow& w, const nlohmann::json& args, bool persist, std::string& errorCode,
                         std::string& errorMessage)
{
    auto fail = [&](const std::string& message) {
        errorCode = "bad_arguments";
        errorMessage = message;
        return false;
    };
    auto onOff = [&](const char* name, bool& value) -> bool {
        const std::string t = argText(args, name);
        if (t.empty() || t == "-") return true;
        if (t == "on" || t == "true" || t == "1") value = true;
        else if (t == "off" || t == "false" || t == "0") value = false;
        else return fail(std::string(name) + ": on or off, got " + t);
        return true;
    };
    auto color = [&](const char* name, std::array<float, 4>& value) -> bool {
        const std::string t = argText(args, name);
        if (t.empty() || t == "-") return true;
        std::array<float, 4> c{ 0.0f, 0.0f, 0.0f, 1.0f };
        unsigned int hex = 0;
        if (t.size() == 7 && t[0] == '#' && std::sscanf(t.c_str() + 1, "%6x", &hex) == 1) {
            c = { ((hex >> 16) & 255) / 255.0f, ((hex >> 8) & 255) / 255.0f, (hex & 255) / 255.0f, value[3] };
        } else if (t.size() == 9 && t[0] == '#' && std::sscanf(t.c_str() + 1, "%8x", &hex) == 1) {
            c = { ((hex >> 24) & 255) / 255.0f, ((hex >> 16) & 255) / 255.0f, ((hex >> 8) & 255) / 255.0f, (hex & 255) / 255.0f };
        } else if (!parseColor(t, c)) {
            return fail(std::string(name) + ": r,g,b,a (0..1) or #rrggbb[aa], got " + t);
        }
        value = c;
        return true;
    };
    auto spacing = [&](const char* name, int& value) -> bool {
        const std::string t = argText(args, name);
        if (t.empty() || t == "-") return true;
        if (!parseSpacing(t, value)) return fail(std::string(name) + ": an integer spacing 1..16384, got " + t);
        return true;
    };
    const ViewerPrefs before = s_prefs;
    ViewerPrefs next = s_prefs;
    if (args.value("grid_defaults", false)) {
        const ViewerPrefs d;
        next.major = d.major;
        next.minor = d.minor;
    }
    if (!onOff("crosshair", next.crosshair) || !onOff("grid", next.grid) || !spacing("major", next.major.spacing)
        || !spacing("minor", next.minor.spacing) || !color("major_color", next.major.color)
        || !color("minor_color", next.minor.color)) {
        return false;
    }
    const std::string cursor = argText(args, "cursor");
    if (!cursor.empty() && cursor != "-") {
        if (cursor == "off" || cursor == "none") {
            w.crossPinned = false;
        } else {
            double x = 0.0, y = 0.0;
            const nlohmann::json& v = args["cursor"];
            if (v.is_array() && v.size() == 2 && v[0].is_number() && v[1].is_number()) {
                x = v[0].get<double>();
                y = v[1].get<double>();
            } else if (std::sscanf(cursor.c_str(), "%lf,%lf", &x, &y) != 2) {
                return fail("cursor: x,y (image pixel) or off, got " + cursor);
            }
            const PixelSet& px = w.active();
            if (x < 0.0 || y < 0.0 || x >= px.width || y >= px.height) return fail("cursor: outside the image");
            w.crossPinned = true;
            w.crossPinX = static_cast<uint32_t>(x);
            w.crossPinY = static_cast<uint32_t>(y);
        }
    }
    s_prefs = next;
    if (persist) {
        commitPrefs(before);
        if (s_prefsDirty) savePrefs();
    }
    return true;
}

} // namespace

bool ImageViewer::script(const ux3d::gltf::Gltf& model, ux3d::renderer::GltfRenderLayer* layer, const nlohmann::json& args,
                         bool persist, nlohmann::json& result, std::string& errorCode, std::string& errorMessage)
{
    ensurePrefsLoaded();
    using json = nlohmann::json;
    auto fail = [&](const char* code, const std::string& message) {
        errorCode = code;
        errorMessage = message;
        return false;
    };

    const long long index = args.value("image", -1LL);
    const ux3d::gltf::Image* image = nullptr;
    for (const auto& candidate : model.getImages()) {
        if (static_cast<long long>(candidate.m_ID) == index) image = &candidate;
    }
    if (image == nullptr) return fail("image_not_found", "no image with index " + std::to_string(index));

    if (args.value("close", false)) {
        auto it = s_windows.find(imageKey(image));
        if (it != s_windows.end()) {
            releaseWindow(*it->second);
            s_windows.erase(it);
        }
        result = { { "closed", true }, { "windows", s_windows.size() } };
        return true;
    }

    // Default: the Inspector's choice (a declared file's packaged normal map is signed-square decoded).
    bool signedSquare = model.isExtensionDeclared("ASOBO_asset_optimized")
        && isPackagedNormalMap(model, image, model.isExtensionDeclared("MSFT_texture_dds"));
    if (args.contains("signed_square") && args["signed_square"].is_boolean()) signedSquare = args["signed_square"].get<bool>();
    // As a click on the Inspector's preview: that preview texture (or the renderer's) is shown until the full one.
    const unsigned int preview = ElementInspector::getImagePreviewTextureId(image, layer, signedSquare);
    ViewerWindow* w = openWindow(model, image, signedSquare, preview);
    if (w == nullptr) return fail("image_not_found", "the image is not part of the model");
    if (!applyOverlayOptions(*w, args, persist, errorCode, errorMessage)) return false;
    if (!applyViewOptions(*w, args, errorCode, errorMessage)) return false;

    auto pair = [&](const char* name, ImVec2& value) -> int {
        const std::string t = argText(args, name);
        if (t.empty() || t == "-") return 0;
        float a = 0.0f, b = 0.0f;
        if (args[name].is_array() && args[name].size() == 2) {
            a = args[name][0].get<float>();
            b = args[name][1].get<float>();
        } else if (std::sscanf(t.c_str(), "%f,%f", &a, &b) != 2) {
            return -1;
        }
        value = ImVec2(a, b);
        return 1;
    };
    const int pos = pair("window_pos", w->requestedPos);
    if (pos < 0) return fail("bad_arguments", "window_pos: x,y");
    if (pos > 0) w->pendingPos = true;
    const int size = pair("window_size", w->requestedSize);
    if (size < 0 || (size > 0 && (w->requestedSize.x < 100.0f || w->requestedSize.y < 100.0f))) {
        return fail("bad_arguments", "window_size: w,h (at least 100,100)");
    }
    if (size > 0) w->pendingSize = true;
    result = stateJson(*w);
    return true;
}

} // namespace ux3d::imgui
