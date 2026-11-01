// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Flag.h"
#include "core/String.h"
#include "core/Vector.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ux3d::gltf {
enum class Extension : uint32_t {
    None = 0,
    PunctualLights = 1u << 0u,
    PbrSpecularGlossinessMaterial = 1u << 1u,
    UnlitMaterial = 1u << 2u,
    TextureBasisU = 1u << 3u,
    TextureTransform = 1u << 4u,
    Clearcoat = 1u << 5u,
    Specular = 1u << 6u,
    DracoMeshCompression = 1u << 7u,
    MeshQuantization = 1u << 8u,
    Sheen = 1u << 9u,
    Ior = 1u << 10u,
    Transmission = 1u << 11u,
    MaterialsVariants = 1u << 12u,
    XMP = 1u << 13u,
    Volume = 1u << 14u,
    Iridescence = 1u << 15u,
    EmissiveStrength = 1u << 16u,
    Audio = 1u << 17u,
    AnimationPointer = 1u << 18u,
    WebP = 1u << 19u,
    TextureDDS = 1u << 20u,
    Anisotropy = 1u << 21u,
};

using Extensions = core::Flag<Extension>;

core::Vector<Extension> getAllExtensions();
core::Vector<core::string> getAllExtensionNames();
core::string extensionToString(Extension _extension);
Extension stringToExtension(core::string& _extensionStr);
bool extensionStringKnown(core::string& _extensionStr);
bool isExtensionRequired(Extension _extension);

// The one list of glTF extensions Glider reads or renders (validator: unsupported extensionsRequired /
// extensionsUsed entries): true when supportedExtensions() has a row for the name. The rows are every name of
// getAllExtensionNames() (KHR_*, EXT_texture_webp, MSFT_texture_dds), EXT_meshopt_compression and
// KHR_meshopt_compression (decoded at load), KHR_node_visibility (hidden nodes), EXT_mesh_gpu_instancing (drawn
// instanced), ux3d_gestaltor (Gestaltor's own storage hints), and every row of asobo::supportTable() (ASOBO_*) that is not "Not yet supported".
bool isExtensionSupported(std::string_view _extensionName);

// One extension Glider supports, with what its switch in File > Settings > glTF Extensions does. The table
// (supportedExtensions) is the one source of truth: isExtensionSupported, the loader's switched-off handling
// (GltfFromJson), the settings groups (GltfExtensionSettings), the registry keys and get_state read it.
struct ExtensionSupport {
    std::string name;        // the glTF extension name; also the registry and settings.ini key of its switch
    std::string vendor;      // the group: "KHR", "EXT", "MSFT", "ASOBO" or another vendor ("ux3d")
    std::string status;      // "Rendered", "Partial", "Read" (decoded at load), "Metadata", "Not applicable"
    std::string description; // what Glider does with it (one line, tooltip)
    std::string offMeans;    // what switching it off does (tooltip, docs)
    // The switch exists (false: ASOBO_asset_optimized, the package layout of the vertex data, always read).
    bool switchable = true;
    // Applied by the renderer (the ASOBO viewport switches): no reload. Every other switch is applied at load: a
    // switched-off extension is read as absent from the file, and a file that requires it is not loaded.
    bool renderTime = false;
    // Off: a file that declares it, even only in extensionsUsed, is not loaded, because its data cannot be read
    // without it (KHR_mesh_quantization).
    bool refuseWhenOff = false;
};

const std::vector<ExtensionSupport>& supportedExtensions();
const ExtensionSupport* findExtensionSupport(std::string_view _extensionName);

}
