// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Viewport image capture, save and copy: the three Viewport menu items and the
// save_viewport_image / copy_viewport_image commands. They read the same display texture
// and use the same readback and PNG writer as the `capture` command and --capture-viewport
// (Diagnostics.cpp), so every route yields identical pixels for the same frame.

#include "Application.h"
#include "imgui/CaptureSettings.h"
#include "imgui/UiProbe.h"
#include "imgui/stb_image_write.h"

#include <portable-file-dialogs.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>
#endif

namespace ux3d::imgui::application {

namespace {

namespace fs = std::filesystem;

std::string pathText(const fs::path& path)
{
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

bool hasPngExtension(const fs::path& path)
{
    std::string ext = pathText(path.extension());
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".png";
}

void appendPngBytes(void* context, void* data, int size)
{
    auto* out = static_cast<std::vector<uint8_t>*>(context);
    const auto* bytes = static_cast<const uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
}

// Save As target. Scripted runs may supply it through GLIDER_TEST_SAVE_FILE (an empty value
// cancels), so the real menu dispatch can be exercised without a native dialog.
std::string chooseSavePath(const char* title, const fs::path& folder, const std::string& fileName, bool scripted)
{
    if (scripted) {
        if (const char* path = std::getenv("GLIDER_TEST_SAVE_FILE")) return path;
    }
    std::error_code ec;
    fs::path start = folder;
    if (!fs::is_directory(start, ec)) start = folder.parent_path(); // Not created until something is saved.
    if (!fs::is_directory(start, ec)) start.clear();
    const fs::path proposal = start.empty() ? fs::u8path(fileName) : start / fs::u8path(fileName);
    return pfd::save_file(title, pathText(proposal), {"PNG image (*.png)", "*.png"}).result();
}

} // namespace

bool Application::viewportImageSource(const ux3d::imgui::ViewportInstance* vp, GLuint& texture, int& width,
                                      int& height) const
{
    texture = 0;
    width = 0;
    height = 0;
    if (vp == nullptr) return false;
    width = vp->sceneFbo.getWidth();
    height = vp->sceneFbo.getHeight();
    texture = vp->cachedDisplayTexture;
    if (texture == 0) texture = vp->overlayFbo.getColorTexture();
    if (texture == 0) texture = vp->sceneFbo.getColorTexture();
    return texture != 0 && width > 0 && height > 0;
}

bool Application::viewportImageAvailable(const ux3d::imgui::ViewportInstance* vp) const
{
    GLuint texture = 0;
    int width = 0, height = 0;
    return vp != nullptr && vp->hasCachedImage && viewportImageSource(vp, texture, width, height);
}

bool Application::saveViewportImage(ux3d::imgui::ViewportInstance* vp, const std::string& path,
                                    ViewportImageResult& result, std::string& error)
{
    result = ViewportImageResult();
    auto fail = [&](const std::string& message) {
        error = message;
        logMessage("[Capture] " + message, ImVec4(1.0f, 0.4f, 0.4f, 1.0f), ux3d::core::LogLevel::Error);
        return false;
    };
    if (vp == nullptr) return fail("Viewport capture failed: no viewport");
    if (loadingActive || !queuedLoadPath.empty()) return fail("Viewport capture failed: a model is loading");
    GLuint texture = 0;
    int width = 0, height = 0;
    if (!viewportImageSource(vp, texture, width, height) || !vp->hasCachedImage) {
        return fail("Viewport capture failed: the viewport has no image yet");
    }

    fs::path target;
    if (path.empty()) {
        // Capture folder, created on demand. An unusable folder is an error: nothing is written elsewhere.
        const fs::path folder = fs::u8path(ux3d::imgui::CaptureSettings::effectiveFolder()).lexically_normal();
        std::error_code ec;
        fs::create_directories(folder, ec);
        if (ec || !fs::is_directory(folder, ec)) {
            return fail("Viewport capture failed: cannot create the capture folder " + pathText(folder));
        }
        target = ux3d::imgui::CaptureSettings::uniquePath(
            folder, ux3d::imgui::CaptureSettings::proposedFileName(currentFilePath, std::chrono::system_clock::now()));
    } else {
        target = fs::u8path(path);
        if (!hasPngExtension(target)) target += ".png"; // The writer picks the format from the extension.
    }

    ux3d::imgui::profiling::CaptureRecord record;
    if (!captureViewportTexture(texture, width, height, pathText(target), "viewport-texture", frameCount, false, 0.0,
                                record)) {
        return fail("Viewport capture failed: cannot write " + pathText(target));
    }
    result.path = pathText(target);
    result.width = width;
    result.height = height;
    result.hash = record.rgbaHash;
    result.viewport = vp->id;
    logMessage("[Capture] Viewport image saved: " + result.path + " (" + std::to_string(width) + "x" +
               std::to_string(height) + ")");
    return true;
}

bool Application::copyViewportImage(ux3d::imgui::ViewportInstance* vp, ViewportImageResult& result,
                                    std::string& error)
{
    result = ViewportImageResult();
    auto fail = [&](const std::string& message) {
        error = message;
        logMessage("[Capture] " + message, ImVec4(1.0f, 0.4f, 0.4f, 1.0f), ux3d::core::LogLevel::Error);
        return false;
    };
    if (vp == nullptr) return fail("Viewport copy failed: no viewport");
    if (loadingActive || !queuedLoadPath.empty()) return fail("Viewport copy failed: a model is loading");
    GLuint texture = 0;
    int width = 0, height = 0;
    if (!viewportImageSource(vp, texture, width, height) || !vp->hasCachedImage) {
        return fail("Viewport copy failed: the viewport has no image yet");
    }
    std::vector<uint8_t> pixels;
    double readbackMs = 0.0;
    if (!readTexturePixels(texture, width, height, pixels, readbackMs)) {
        return fail("Viewport copy failed: cannot read the viewport image");
    }
    result.width = width;
    result.height = height;
    result.hash = hashBytesFnv1a64(pixels.data(), pixels.size());
    result.viewport = vp->id;

#ifdef _WIN32
    // PNG keeps full quality (and alpha) for applications that ask for it.
    std::vector<uint8_t> png;
    stbi_flip_vertically_on_write(1); // GL rows run bottom to top.
    if (stbi_write_png_to_func(appendPngBytes, &png, width, height, 4, pixels.data(), width * 4) == 0 || png.empty()) {
        return fail("Viewport copy failed: cannot encode the PNG");
    }
    // CF_DIB: 24-bit BGR, bottom-up (positive height), rows padded to 4 bytes; the readback
    // is already bottom row first. Alpha is dropped: applications that only know DIBs show
    // the viewport as displayed.
    const size_t stride = (static_cast<size_t>(width) * 3u + 3u) & ~static_cast<size_t>(3);
    const size_t dibBytes = sizeof(BITMAPINFOHEADER) + stride * static_cast<size_t>(height);

    HGLOBAL dibMemory = GlobalAlloc(GMEM_MOVEABLE, dibBytes);
    HGLOBAL pngMemory = GlobalAlloc(GMEM_MOVEABLE, png.size());
    if (dibMemory == nullptr || pngMemory == nullptr) {
        if (dibMemory != nullptr) GlobalFree(dibMemory);
        if (pngMemory != nullptr) GlobalFree(pngMemory);
        return fail("Viewport copy failed: out of memory");
    }
    if (auto* dib = static_cast<uint8_t*>(GlobalLock(dibMemory))) {
        std::memset(dib, 0, dibBytes);
        auto* header = reinterpret_cast<BITMAPINFOHEADER*>(dib);
        header->biSize = sizeof(BITMAPINFOHEADER);
        header->biWidth = width;
        header->biHeight = height;
        header->biPlanes = 1;
        header->biBitCount = 24;
        header->biCompression = BI_RGB;
        header->biSizeImage = static_cast<DWORD>(stride * static_cast<size_t>(height));
        uint8_t* rows = dib + sizeof(BITMAPINFOHEADER);
        for (int y = 0; y < height; ++y) {
            const uint8_t* src = pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u;
            uint8_t* dst = rows + static_cast<size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                dst[x * 3 + 0] = src[x * 4 + 2];
                dst[x * 3 + 1] = src[x * 4 + 1];
                dst[x * 3 + 2] = src[x * 4 + 0];
            }
        }
        GlobalUnlock(dibMemory);
    }
    if (auto* dest = static_cast<uint8_t*>(GlobalLock(pngMemory))) {
        std::memcpy(dest, png.data(), png.size());
        GlobalUnlock(pngMemory);
    }

    const UINT pngFormat = RegisterClipboardFormatA("PNG");
    HWND owner = window != nullptr ? glfwGetWin32Window(window) : nullptr;
    bool opened = false;
    for (int attempt = 0; attempt < 20 && !opened; ++attempt) {
        opened = OpenClipboard(owner) != 0;
        if (!opened) Sleep(10); // Another application holds the clipboard for a moment.
    }
    if (!opened) {
        GlobalFree(dibMemory);
        GlobalFree(pngMemory);
        return fail("Viewport copy failed: the clipboard is in use by another application");
    }
    EmptyClipboard();
    bool ok = SetClipboardData(CF_DIB, dibMemory) != nullptr;
    if (!ok) GlobalFree(dibMemory);
    if (pngFormat != 0 && SetClipboardData(pngFormat, pngMemory) != nullptr) {
        // Owned by the clipboard now.
    } else {
        GlobalFree(pngMemory);
    }
    CloseClipboard();
    if (!ok) return fail("Viewport copy failed: cannot set the clipboard data");
    logMessage("[Capture] Viewport image copied to the clipboard (" + std::to_string(width) + "x" +
               std::to_string(height) + ")");
    return true;
#else
    return fail("Viewport copy is only available on Windows");
#endif
}

void Application::saveViewportViewAs(ux3d::imgui::ViewportInstance* vp)
{
    const fs::path folder = fs::u8path(ux3d::imgui::CaptureSettings::effectiveFolder());
    const std::string name =
        ux3d::imgui::CaptureSettings::proposedFileName(currentFilePath, std::chrono::system_clock::now());
    const std::string chosen = chooseSavePath("Save Viewport View", folder, name, uiScenario.active());
    if (chosen.empty()) return; // Cancelled.
    ViewportImageResult result;
    std::string error;
    saveViewportImage(vp, chosen, result, error);
}

void Application::drawViewportImageMenuItems()
{
    const bool ready = activeViewport != nullptr && !loadingActive && queuedLoadPath.empty() &&
                       viewportImageAvailable(activeViewport);
    ViewportImageResult result;
    std::string error;
    if (ImGui::MenuItem(ICON_MDI_CAMERA_IRIS " Capture Viewport", nullptr, false, ready)) {
        saveViewportImage(activeViewport, "", result, error);
    }
    ux3d::imgui::UiProbe::item("viewport.capture", 0);
    if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE_OUTLINE " Save Viewport View...", nullptr, false, ready)) {
        saveViewportViewAs(activeViewport);
    }
    ux3d::imgui::UiProbe::item("viewport.save_view", 0);
    if (ImGui::MenuItem(ICON_MDI_CONTENT_COPY " Copy Viewport View", nullptr, false, ready)) {
        copyViewportImage(activeViewport, result, error);
    }
    ux3d::imgui::UiProbe::item("viewport.copy_view", 0);
}

} // namespace ux3d::imgui::application
