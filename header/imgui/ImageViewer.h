// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace ux3d::gltf {
class Gltf;
class Image;
class Material;
class TextureInfo;
} // namespace ux3d::gltf
namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {

// Image Viewer: a dockable window per image path that shows an image at full resolution for analysis
// (docs/ARCHITECTURE.md §3.5b). The pixels are a private full-size CPU copy decoded from the file's bytes
// with the values the file stores (8/16-bit integers, signed BC4/BC5, float), uploaded once in a matching GL
// format. Top: the colour picker (swatch + one level bar and stored value per channel) and the per-channel
// histogram with the low/high levels, split by a draggable splitter. Bottom status row: zoom, channel toggles,
// transparency background, Raw (normal maps shown decoded), the cursor pixel and the image facts. Nothing is
// persisted. Works in a detached OS window (multi-viewport): it caches no VAO or FBO across GL contexts.
// A window can carry one read-only UV layout overlay (opened from a material's texture slot): the triangle edges
// of every primitive with that material in the slot's TEXCOORD set, drawn over the image in UV space.
class ImageViewer {
public:
    // What a UV layout overlay shows: the material, the slot's TEXCOORD set (KHR_texture_transform's override
    // included) and its transform (column-major 3x3, as TextureInfo::calculateTransformMatrix).
    struct UvLayoutRequest {
        const ux3d::gltf::Material* material = nullptr;
        std::string slot; // display name, e.g. "Base Color Texture"
        uint32_t texCoord = 0;
        bool hasTransform = false;
        std::array<double, 9> transform{ 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
    };
    static UvLayoutRequest uvLayoutRequest(const ux3d::gltf::Material* material, const char* slot,
                                           const ux3d::gltf::TextureInfo& info);

    // Opens the window of the image's path, or focuses it (and switches it to this image object).
    // previewTexture: an existing GL texture of the image (the Inspector's preview or the renderer's), drawn at
    // once while the full-resolution pixels load on a worker thread; 0 draws a placeholder until then.
    static void open(const ux3d::gltf::Gltf& model, const ux3d::gltf::Image* image, bool signedSquareNormal,
                     unsigned int previewTexture = 0);
    // Opens the image's window (as open) and attaches the UV layout overlay. The same material, TEXCOORD set
    // and transform keep the attached overlay; anything else replaces it. layer: the drawn index slices
    // (ASOBO_primitive); null reads every primitive's whole index accessor.
    static void openUvLayout(const ux3d::gltf::Gltf& model, const ux3d::renderer::GltfRenderLayer* layer,
                             const ux3d::gltf::Image* image, bool signedSquareNormal, const UvLayoutRequest& request,
                             unsigned int previewTexture = 0);

    // Submits every open window; call once per frame inside the ImGui frame. areaShows(windowName) false
    // hides a docked viewer while Maximize Area shows another panel (floating viewers stay).
    static void render(const ux3d::gltf::Gltf& model, const std::function<bool(const char* windowName)>& areaShows);

    // Closes every window and frees its GL texture and pixel copy (a new model).
    static void closeAll();
    // The images were decoded again (texture reload): every window re-resolves its key and decodes again.
    static void reloadAll();
    // closeAll plus the shared GL program and samplers (shutdown).
    static void shutdown();
    static size_t windowCount();

    // Agent / script hook (registry command image_viewer, docs/LIVE_SCRIPT_CONTROL.md). Opens the viewer of
    // glTF image args["image"], applies the optional view arguments and fills the viewer state.
    // layer: for the preview texture (as the Inspector's). Options that need the pixels wait until they are loaded.
    // persist: crosshair / grid preference changes may be written to settings.ini (pipe: set_persist(true)).
    static bool script(const ux3d::gltf::Gltf& model, ux3d::renderer::GltfRenderLayer* layer, const nlohmann::json& args,
                       bool persist, nlohmann::json& result, std::string& errorCode, std::string& errorMessage);
    // Registry command image_viewer_wait: the state of image imageIndex's window and whether it is ready (pixels,
    // texture, histogram and UV overlay). False when the image has no window.
    static bool waitState(const ux3d::gltf::Gltf& model, long long imageIndex, nlohmann::json& state, bool& ready,
                          std::string& errorCode, std::string& errorMessage);
    // Registry command uv_layout: opens the UV layout of material args["material"], slot args["slot"]
    // (baseColor, metallicRoughness, normal, occlusion, emissive) and sets wrap / fill / transform / show.
    static bool uvLayoutScript(const ux3d::gltf::Gltf& model, ux3d::renderer::GltfRenderLayer* layer,
                               const nlohmann::json& args, nlohmann::json& result, std::string& errorCode,
                               std::string& errorMessage);
};

} // namespace ux3d::imgui
