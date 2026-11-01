#include "gltf/Extensions.h"
#include "core/Algorithms.h"
#include "core/Logger.h"
#include "gltf/MeshoptCompression.h"
#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/extensions/AsoboSupport.h"

ux3d::core::Vector<ux3d::gltf::Extension> ux3d::gltf::getAllExtensions()
{
    return {
        Extension::PunctualLights,
        Extension::PbrSpecularGlossinessMaterial,
        Extension::UnlitMaterial,
        Extension::TextureBasisU,
        Extension::TextureTransform,
        Extension::Clearcoat,
        Extension::Specular,
        Extension::DracoMeshCompression,
        Extension::MeshQuantization,
        Extension::Sheen,
        Extension::Ior,
        Extension::Transmission,
        Extension::MaterialsVariants,
        Extension::XMP,
        Extension::Volume,
        Extension::Iridescence,
        Extension::EmissiveStrength,
        Extension::AnimationPointer,
        Extension::Audio,
        Extension::WebP,
        Extension::TextureDDS,
        Extension::Anisotropy,
    };
}

ux3d::core::Vector<ux3d::core::string> ux3d::gltf::getAllExtensionNames()
{
    return {
        extensionToString(Extension::PunctualLights),
        extensionToString(Extension::PbrSpecularGlossinessMaterial),
        extensionToString(Extension::UnlitMaterial),
        extensionToString(Extension::TextureBasisU),
        extensionToString(Extension::TextureTransform),
        extensionToString(Extension::Clearcoat),
        extensionToString(Extension::Specular),
        extensionToString(Extension::DracoMeshCompression),
        extensionToString(Extension::MeshQuantization),
        extensionToString(Extension::Sheen),
        extensionToString(Extension::Ior),
        extensionToString(Extension::Transmission),
        extensionToString(Extension::MaterialsVariants),
        extensionToString(Extension::XMP),
        extensionToString(Extension::Volume),
        extensionToString(Extension::Iridescence),
        extensionToString(Extension::EmissiveStrength),
        extensionToString(Extension::AnimationPointer),
        extensionToString(Extension::Audio),
        extensionToString(Extension::WebP),
        extensionToString(Extension::TextureDDS),
        extensionToString(Extension::Anisotropy),
    };
}

ux3d::core::string ux3d::gltf::extensionToString(Extension _extension)
{
    switch (_extension) {
    case Extension::None:
        break;
    case Extension::PunctualLights:
        return "KHR_lights_punctual";
    case Extension::PbrSpecularGlossinessMaterial:
        return "KHR_materials_pbrSpecularGlossiness";
    case Extension::UnlitMaterial:
        return "KHR_materials_unlit";
    case Extension::TextureBasisU:
        return "KHR_texture_basisu";
    case Extension::WebP:
        return "EXT_texture_webp";
    case Extension::TextureDDS:
        return "MSFT_texture_dds";
    case Extension::TextureTransform:
        return "KHR_texture_transform";
    case Extension::Clearcoat:
        return "KHR_materials_clearcoat";
    case Extension::Specular:
        return "KHR_materials_specular";
    case Extension::DracoMeshCompression:
        return "KHR_draco_mesh_compression";
    case Extension::MeshQuantization:
        return "KHR_mesh_quantization";
    case Extension::Sheen:
        return "KHR_materials_sheen";
    case Extension::Ior:
        return "KHR_materials_ior";
    case Extension::Transmission:
        return "KHR_materials_transmission";
    case Extension::MaterialsVariants:
        return "KHR_materials_variants";
    case Extension::XMP:
        return "KHR_xmp_json_ld";
    case Extension::Volume:
        return "KHR_materials_volume";
    case Extension::Iridescence:
        return "KHR_materials_iridescence";
    case Extension::EmissiveStrength:
        return "KHR_materials_emissive_strength";
    case Extension::AnimationPointer:
        return "KHR_animation_pointer";
    case Extension::Audio:
        return "KHR_audio";
    case Extension::Anisotropy:
        return "KHR_materials_anisotropy";
    }

    core::logError("%u is not a valid extension value, it might contain multiple extension flags",
        static_cast<uint32_t>(_extension));
    return "";
}

ux3d::gltf::Extension ux3d::gltf::stringToExtension(core::string& _extensionStr)
{
    if (_extensionStr == "KHR_lights_punctual") {
        return Extension::PunctualLights;
    }
    if (_extensionStr == "KHR_materials_pbrSpecularGlossiness") {
        return Extension::PbrSpecularGlossinessMaterial;
    }
    if (_extensionStr == "KHR_materials_unlit") {
        return Extension::UnlitMaterial;
    }
    if (_extensionStr == "KHR_texture_basisu") {
        return Extension::TextureBasisU;
    }
    if (_extensionStr == "EXT_texture_webp") {
        return Extension::WebP;
    }
    if (_extensionStr == "MSFT_texture_dds") {
        return Extension::TextureDDS;
    }
    if (_extensionStr == "KHR_texture_transform") {
        return Extension::TextureTransform;
    }
    if (_extensionStr == "KHR_materials_clearcoat") {
        return Extension::Clearcoat;
    }
    if (_extensionStr == "KHR_materials_specular") {
        return Extension::Specular;
    }
    if (_extensionStr == "KHR_materials_emissive_strength") {
        return Extension::EmissiveStrength;
    }
    if (_extensionStr == "KHR_draco_mesh_compression") {
        return Extension::DracoMeshCompression;
    }
    if (_extensionStr == "KHR_mesh_quantization") {
        return Extension::MeshQuantization;
    }
    if (_extensionStr == "KHR_materials_sheen") {
        return Extension::Sheen;
    }
    if (_extensionStr == "KHR_materials_ior") {
        return Extension::Ior;
    }
    if (_extensionStr == "KHR_materials_transmission") {
        return Extension::Transmission;
    }
    if (_extensionStr == "KHR_materials_variants") {
        return Extension::MaterialsVariants;
    }
    if (_extensionStr == "KHR_xmp_json_ld") {
        return Extension::XMP;
    }
    if (_extensionStr == "KHR_materials_volume") {
        return Extension::Volume;
    }
    if (_extensionStr == "KHR_materials_iridescence") {
        return Extension::Iridescence;
    }
    if (_extensionStr == "KHR_materials_anisotropy") {
        return Extension::Anisotropy;
    }
    if (_extensionStr == "KHR_animation_pointer") {
        return Extension::AnimationPointer;
    }
    if (_extensionStr == "KHR_audio") {
        return Extension::Audio;
    }

    if (_extensionStr != "ux3d_gestaltor") {
        core::debugLog(core::LogLevel::Warning, "%s is not a known extension name", _extensionStr.c_str());
    }

    return Extension::None;
}
bool ux3d::gltf::extensionStringKnown(core::string& _extensionStr)
{
    auto extensionsNames = getAllExtensionNames();
    if (core::find(extensionsNames.begin(), extensionsNames.end(), _extensionStr) != extensionsNames.end()) {
        return true;
    }
    return false;
}

bool ux3d::gltf::isExtensionRequired(Extension _extension)
{
    if (_extension == Extension::Audio || _extension == Extension::PunctualLights || _extension == Extension::MaterialsVariants || _extension == Extension::XMP || _extension == Extension::PbrSpecularGlossinessMaterial || _extension == Extension::UnlitMaterial || _extension == Extension::Clearcoat || _extension == Extension::Specular || _extension == Extension::Sheen || _extension == Extension::Ior || _extension == Extension::Transmission || _extension == Extension::Volume || _extension == Extension::EmissiveStrength || _extension == Extension::WebP || _extension == Extension::TextureDDS || _extension == Extension::Anisotropy) {
        return false;
    }

    if (_extension == Extension::DracoMeshCompression || _extension == Extension::MeshQuantization || _extension == Extension::TextureTransform || _extension == Extension::TextureBasisU) {
        return true;
    }

    return true;
}

namespace {

using ux3d::gltf::ExtensionSupport;

ExtensionSupport row(const char* name, const char* vendor, const char* status, const char* description,
    const char* offMeans, bool refuseWhenOff = false)
{
    ExtensionSupport info;
    info.name = name;
    info.vendor = vendor;
    info.status = status;
    info.description = description;
    info.offMeans = offMeans;
    info.refuseWhenOff = refuseWhenOff;
    return info;
}

std::vector<ExtensionSupport> buildSupportTable()
{
    constexpr const char* materialOff = "Read as absent: its parameters are ignored and the material renders as its "
                                        "core glTF material.";
    constexpr const char* textureOff = "Read as absent: each texture uses its core source image; a texture without "
                                       "one has no image. A file that requires it is not loaded.";
    std::vector<ExtensionSupport> table = {
        // Khronos
        row("KHR_animation_pointer", "KHR", "Rendered", "Animates material, light, camera and node properties "
            "through JSON pointers.", "Channels that target a JSON pointer are skipped; node channels still play."),
        row("KHR_audio", "KHR", "Metadata", "Audio sources and emitters are read (Inspector); Glider plays no sound.",
            "Not read: no audio sources or emitters."),
        row("KHR_draco_mesh_compression", "KHR", "Read", "Draco-compressed primitives are decoded at load.",
            "Primitives use their uncompressed fallback accessors (most Draco files have none and require the "
            "extension). A file that requires it is not loaded."),
        row("KHR_lights_punctual", "KHR", "Rendered", "Point, spot and directional lights.",
            "Read as absent: no lights are created from the file."),
        row("KHR_materials_anisotropy", "KHR", "Rendered", "Anisotropic specular lobe.", materialOff),
        row("KHR_materials_clearcoat", "KHR", "Rendered", "Clear coat layer.", materialOff),
        row("KHR_materials_emissive_strength", "KHR", "Rendered", "Emissive strength above 1 (HDR, bloom).",
            materialOff),
        row("KHR_materials_ior", "KHR", "Rendered", "Index of refraction.", materialOff),
        row("KHR_materials_iridescence", "KHR", "Rendered", "Thin-film iridescence.", materialOff),
        row("KHR_materials_pbrSpecularGlossiness", "KHR", "Rendered", "Specular-glossiness materials (archived "
            "extension).", "Read as absent: the materials use their core metallic-roughness values."),
        row("KHR_materials_sheen", "KHR", "Rendered", "Sheen (cloth) layer.", materialOff),
        row("KHR_materials_specular", "KHR", "Rendered", "Specular strength and colour.", materialOff),
        row("KHR_materials_transmission", "KHR", "Rendered", "Transmission (thin transparent surfaces).", materialOff),
        row("KHR_materials_unlit", "KHR", "Rendered", "Unlit (constant colour) materials.",
            "Read as absent: the materials are shaded as core PBR materials."),
        row("KHR_materials_variants", "KHR", "Rendered", "Material variants, selectable in Viewport Settings.",
            "Read as absent: no variants; every primitive shows its default material."),
        row("KHR_materials_volume", "KHR", "Rendered", "Volume attenuation and thickness (with transmission).",
            materialOff),
        row("KHR_mesh_quantization", "KHR", "Read", "Quantized (integer) vertex attributes.",
            "Cannot be ignored: quantized attributes cannot be read without it, so a file that declares it is not "
            "loaded.", true),
        row("KHR_meshopt_compression", "KHR", "Read", "meshoptimizer-compressed bufferViews are decoded at load "
            "(the Khronos name of EXT_meshopt_compression).", "Compressed bufferViews are not decoded; the "
            "uncompressed fallback buffers are read. A file that requires it is not loaded."),
        row("KHR_texture_basisu", "KHR", "Rendered", "KTX2 / Basis Universal textures.", textureOff),
        row("KHR_texture_transform", "KHR", "Rendered", "Texture coordinate offset, rotation and scale.",
            "Read as absent: texture coordinates are used untransformed. A file that requires it is not loaded."),
        row("KHR_xmp_json_ld", "KHR", "Metadata", "XMP metadata packets (Inspector).", "Not read: no XMP metadata."),
        // EXT
        row("EXT_mesh_gpu_instancing", "EXT", "Rendered", "One mesh drawn per instance (GPU instancing).",
            "Read as absent: each instanced node draws its mesh once, with the node's own transform."),
        row("EXT_meshopt_compression", "EXT", "Read", "meshoptimizer-compressed bufferViews are decoded at load.",
            "Compressed bufferViews are not decoded; the uncompressed fallback buffers are read. A file that "
            "requires it is not loaded."),
        row("EXT_texture_webp", "EXT", "Rendered", "WebP textures.", textureOff),
        // Microsoft
        row("MSFT_texture_dds", "MSFT", "Rendered", "DDS textures (preferred over the core source, with automatic "
            "fallback).", textureOff),
        // Other vendors
        row("ux3d_gestaltor", "ux3d", "Metadata", "UX3D Gestaltor's own storage hints (for example an animation's "
            "playing state).", "Not read: the hints are ignored (animations use their default playing state)."),
    };
    namespace asobo = ux3d::gltf::asobo;
    for (const asobo::SupportInfo& info : asobo::supportTable()) {
        if (!asobo::isAsoboExtension(info.name) || info.status == asobo::Support::NotYetSupported) {
            continue;
        }
        ExtensionSupport entry;
        entry.name = std::string(info.name);
        entry.vendor = "ASOBO";
        entry.status = asobo::supportLabel(info.status);
        entry.description = info.note.empty() ? std::string("Microsoft Flight Simulator extension.")
                                              : std::string(info.note);
        // The ASOBO viewport switches (A0.2) keep their meaning: applied by the renderer, the file is not reloaded.
        entry.renderTime = (info.status == asobo::Support::Rendered || info.status == asobo::Support::Partial)
            && info.viewportSwitch;
        if (entry.renderTime) {
            entry.offMeans = "Rendered as if the file had no " + entry.name + " (applied while rendering; the file "
                             "is not reloaded). The MSFS (ASOBO) master switch turns it off too.";
        } else if (!info.viewportSwitch) {
            // ASOBO_asset_optimized: how the package stores its vertex data (winding, packed attributes). Reading the
            // file without it would read other geometry, and --asobo-disable keeps it applied (A1.9).
            entry.switchable = false;
            entry.offMeans = "No switch: it describes how the file stores its data and is always read.";
        } else {
            entry.offMeans = "Not read: the Inspector does not list it. Applied at load: switching it reloads the "
                             "open file.";
        }
        table.push_back(std::move(entry));
    }
    return table;
}

} // namespace

const std::vector<ux3d::gltf::ExtensionSupport>& ux3d::gltf::supportedExtensions()
{
    static const std::vector<ExtensionSupport> table = buildSupportTable();
    return table;
}

const ux3d::gltf::ExtensionSupport* ux3d::gltf::findExtensionSupport(std::string_view _extensionName)
{
    for (const ExtensionSupport& info : supportedExtensions()) {
        if (info.name == _extensionName) {
            return &info;
        }
    }
    return nullptr;
}

bool ux3d::gltf::isExtensionSupported(std::string_view _extensionName)
{
    return findExtensionSupport(_extensionName) != nullptr;
}
