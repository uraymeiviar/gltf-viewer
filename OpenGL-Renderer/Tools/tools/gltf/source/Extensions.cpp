#include "gltf/Extensions.h"
#include "core/Algorithms.h"
#include "core/Logger.h"
#include "gltf/MeshoptCompression.h"
#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/extensions/AsoboSupport.h"
#include "gltf/Gltf.h"
#include "gltf/extensions/MsftExtensions.h"
#include "gltf/extensions/TilesExtensions.h"
#include <algorithm>
#include <nlohmann/json.hpp>

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
        Extension::Dispersion,
        Extension::DiffuseTransmission,
        Extension::DiffuseRoughness,
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
        extensionToString(Extension::Dispersion),
        extensionToString(Extension::DiffuseTransmission),
        extensionToString(Extension::DiffuseRoughness),
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
    case Extension::Dispersion:
        return "KHR_materials_dispersion";
    case Extension::DiffuseTransmission:
        return "KHR_materials_diffuse_transmission";
    case Extension::DiffuseRoughness:
        return "KHR_materials_diffuse_roughness";
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
    if (_extensionStr == "KHR_materials_dispersion") {
        return Extension::Dispersion;
    }
    if (_extensionStr == "KHR_materials_diffuse_transmission") {
        return Extension::DiffuseTransmission;
    }
    if (_extensionStr == "KHR_materials_diffuse_roughness") {
        return Extension::DiffuseRoughness;
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
    if (_extension == Extension::Audio || _extension == Extension::PunctualLights || _extension == Extension::MaterialsVariants || _extension == Extension::XMP || _extension == Extension::PbrSpecularGlossinessMaterial || _extension == Extension::UnlitMaterial || _extension == Extension::Clearcoat || _extension == Extension::Specular || _extension == Extension::Sheen || _extension == Extension::Ior || _extension == Extension::Transmission || _extension == Extension::Volume || _extension == Extension::EmissiveStrength || _extension == Extension::WebP || _extension == Extension::TextureDDS || _extension == Extension::Anisotropy || _extension == Extension::Dispersion || _extension == Extension::DiffuseTransmission || _extension == Extension::DiffuseRoughness) {
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
        row("KHR_materials_diffuse_roughness", "KHR", "Rendered", "Diffuse roughness: energy-preserving Oren-Nayar "
            "(EON) diffuse lobe for the sun and punctual lights; the IBL diffuse stays Lambert (draft extension).",
            materialOff),
        row("KHR_materials_diffuse_transmission", "KHR", "Rendered", "Diffuse transmission through thin surfaces: "
            "light from behind (sun, punctual lights, IBL) shows on the front, tinted by the transmission colour.",
            materialOff),
        row("KHR_materials_dispersion", "KHR", "Rendered", "Chromatic dispersion of volumetric refraction "
            "(transmission with volume): one IOR per colour channel.", materialOff),
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
        row("KHR_node_visibility", "KHR", "Rendered", "Nodes the file hides (visible: false), with their subtrees, "
            "are not drawn, picked or framed; animatable through KHR_animation_pointer.",
            "Read as absent: every node is visible. A file that requires it is not loaded."),
        row("KHR_node_hoverability", "KHR", "Metadata", "Nodes the file marks hoverable: false are listed (Inspector, "
            "hierarchy); Glider has no viewport hover highlight to disable.", "Read as absent: every node is hoverable."),
        row("KHR_node_selectability", "KHR", "Rendered", "Nodes the file marks selectable: false (with their "
            "subtrees) are not picked by a viewport click (it selects what is behind); the Scene Hierarchy still "
            "selects them. Animatable through KHR_animation_pointer.",
            "Read as absent: every node is selectable. A file that requires it is not loaded."),
        row("KHR_texture_basisu", "KHR", "Rendered", "KTX2 / Basis Universal textures.", textureOff),
        row("KHR_texture_transform", "KHR", "Rendered", "Texture coordinate offset, rotation and scale.",
            "Read as absent: texture coordinates are used untransformed. A file that requires it is not loaded."),
        row("KHR_xmp_json_ld", "KHR", "Metadata", "XMP metadata packets (Inspector).", "Not read: no XMP metadata."),
        // EXT
        row("EXT_instance_features", "EXT", "Metadata", "3D Tiles: feature IDs of GPU instances (Inspector: counts, "
            "labels, property tables); not rendered.", "Not read: the Inspector does not list the instance feature IDs. "
            "A file that requires it is not loaded."),
        row("EXT_mesh_features", "EXT", "Metadata", "3D Tiles: feature ID attributes / textures of primitives "
            "(Inspector: counts, sources, property tables); the geometry renders as plain glTF.",
            "Not read: the Inspector does not list the feature IDs. A file that requires it is not loaded."),
        row("EXT_mesh_gpu_instancing", "EXT", "Rendered", "One mesh drawn per instance (GPU instancing).",
            "Read as absent: each instanced node draws its mesh once, with the node's own transform."),
        row("EXT_meshopt_compression", "EXT", "Read", "meshoptimizer-compressed bufferViews are decoded at load.",
            "Compressed bufferViews are not decoded; the uncompressed fallback buffers are read. A file that "
            "requires it is not loaded."),
        row("EXT_structural_metadata", "EXT", "Metadata", "3D Tiles: metadata schema, property tables, textures and "
            "attributes (Statistics / Inspector: names and counts); not rendered.",
            "Not read: no metadata is listed. A file that requires it is not loaded."),
        row("EXT_texture_webp", "EXT", "Rendered", "WebP textures.", textureOff),
        // Microsoft
        row("MSFT_lod", "MSFT", "Metadata", "Levels of detail: the highest LOD (the node or material the file "
            "draws) renders as before; the Inspector lists the LOD chain and the screen-coverage hints. No LOD "
            "switching.", "Not read: the Inspector does not list the LOD chain (rendering is unchanged)."),
        row("MSFT_packing_normalRoughnessMetallic", "MSFT", "Metadata", "Recognised; the packed normal / roughness / "
            "metallic texture is not used (the core textures are rendered).", "Not read (rendering is unchanged). "
            "A file that requires it is not loaded."),
        row("MSFT_packing_occlusionRoughnessMetallic", "MSFT", "Partial", "The packed occlusion / roughness / metallic "
            "texture (R / G / B, the core channel layout) is used where the material has no core metallic-roughness "
            "or occlusion texture; roughnessMetallicOcclusionTexture and the two-channel normalTexture are not used.",
            "Read as absent: only the core textures are used. A file that requires it is not loaded."),
        row("MSFT_texture_dds", "MSFT", "Rendered", "DDS textures (preferred over the core source, with automatic "
            "fallback).", textureOff),
        // Cesium (3D Tiles glb payloads)
        row("CESIUM_RTC", "CESIUM", "Partial", "Positions relative to a center (ECEF): the scene is rendered in that "
            "relative-to-center frame, near the origin; the center is shown (Statistics) but not added.",
            "Not read: the center is not shown (rendering is unchanged). A file that requires it is not loaded."),
        row("CESIUM_primitive_outline", "CESIUM", "Metadata", "Outline edge indices of primitives are recognised and "
            "counted; the outline edges are not drawn.", "Not read: outlines are not counted. A file that requires it "
            "is not loaded."),
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
        } else if (info.loadSwitch) {
            entry.offMeans = "Read as absent: material animation channels are not played; node channels still play. "
                             "Applied at load: switching it reloads the open file.";
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

// ---------------------------------------------------------------------------------------------------------------------
// 3D Tiles / Cesium extensions (gltf/extensions/TilesExtensions.h): read-only summaries of the unknown-extension JSON
// the loader keeps on the root, primitives and nodes.
// ---------------------------------------------------------------------------------------------------------------------

namespace {

nlohmann::json parseOrNull(const std::string& text)
{
    try {
        return nlohmann::json::parse(text);
    } catch (...) {
        return nlohmann::json();
    }
}

const std::string* findUnknown(const ux3d::gltf::GltfObject& object, const char* name)
{
    const auto& extensions = object.getUnknownExtensions();
    const auto it = extensions.find(name);
    return it == extensions.end() ? nullptr : &it->second;
}

}

bool ux3d::gltf::tiles::isTilesExtension(std::string_view name)
{
    return name == StructuralMetadata || name == MeshFeatures || name == InstanceFeatures || name == PrimitiveOutline
        || name == Rtc;
}

std::vector<ux3d::gltf::tiles::FeatureIdSet> ux3d::gltf::tiles::parseFeatureIds(const std::string& extensionJson, bool instances)
{
    std::vector<FeatureIdSet> sets;
    const nlohmann::json json = parseOrNull(extensionJson);
    if (!json.is_object() || !json.contains("featureIds") || !json["featureIds"].is_array()) {
        return sets;
    }
    for (const nlohmann::json& entry : json["featureIds"]) {
        if (!entry.is_object()) {
            continue;
        }
        FeatureIdSet set;
        if (entry.contains("featureCount") && entry["featureCount"].is_number_unsigned()) {
            set.featureCount = entry["featureCount"].get<uint64_t>();
        }
        if (entry.contains("attribute") && entry["attribute"].is_number_integer()) {
            set.source = std::string("attribute _FEATURE_ID_") + std::to_string(entry["attribute"].get<int64_t>());
        } else if (!instances && entry.contains("texture") && entry["texture"].is_object()) {
            const nlohmann::json& texture = entry["texture"];
            set.source = "texture " + (texture.contains("index") && texture["index"].is_number_integer()
                                              ? std::to_string(texture["index"].get<int64_t>())
                                              : std::string("?"));
        } else {
            set.source = instances ? "implicit (instance index)" : "implicit (vertex index)";
        }
        if (entry.contains("propertyTable") && entry["propertyTable"].is_number_integer()) {
            set.propertyTable = entry["propertyTable"].get<int64_t>();
        }
        if (entry.contains("label") && entry["label"].is_string()) {
            set.label = entry["label"].get<std::string>();
        }
        if (entry.contains("nullFeatureId") && entry["nullFeatureId"].is_number_integer()) {
            set.nullFeatureId = entry["nullFeatureId"].get<int64_t>();
        }
        sets.push_back(std::move(set));
    }
    return sets;
}

ux3d::gltf::tiles::Summary ux3d::gltf::tiles::summarize(const Gltf& gltf)
{
    Summary summary;
    if (const std::string* text = findUnknown(gltf, Rtc)) {
        const nlohmann::json json = parseOrNull(*text);
        if (json.is_object() && json.contains("center") && json["center"].is_array() && json["center"].size() == 3
            && json["center"][0].is_number() && json["center"][1].is_number() && json["center"][2].is_number()) {
            summary.rtcCenter = math::dvec3(json["center"][0].get<double>(), json["center"][1].get<double>(),
                json["center"][2].get<double>());
        }
    }
    if (const std::string* text = findUnknown(gltf, StructuralMetadata)) {
        summary.structuralMetadata = true;
        const nlohmann::json json = parseOrNull(*text);
        if (json.is_object()) {
            if (json.contains("schemaUri") && json["schemaUri"].is_string()) {
                summary.schemaUri = json["schemaUri"].get<std::string>();
            }
            if (json.contains("schema") && json["schema"].is_object()) {
                const nlohmann::json& schema = json["schema"];
                if (schema.contains("id") && schema["id"].is_string()) summary.schemaId = schema["id"].get<std::string>();
                if (schema.contains("name") && schema["name"].is_string()) summary.schemaName = schema["name"].get<std::string>();
                if (schema.contains("classes") && schema["classes"].is_object()) {
                    for (const auto& [name, value] : schema["classes"].items()) {
                        summary.classNames.push_back(name);
                    }
                }
                if (schema.contains("enums") && schema["enums"].is_object()) {
                    summary.enums = static_cast<uint32_t>(schema["enums"].size());
                }
            }
            if (json.contains("propertyTables") && json["propertyTables"].is_array()) {
                for (const nlohmann::json& table : json["propertyTables"]) {
                    PropertyTable entry;
                    if (table.is_object()) {
                        if (table.contains("name") && table["name"].is_string()) entry.name = table["name"].get<std::string>();
                        if (table.contains("class") && table["class"].is_string()) entry.className = table["class"].get<std::string>();
                        if (table.contains("count") && table["count"].is_number_unsigned()) entry.count = table["count"].get<uint64_t>();
                        if (table.contains("properties") && table["properties"].is_object()) {
                            for (const auto& [name, value] : table["properties"].items()) {
                                entry.properties.push_back(name);
                            }
                        }
                    }
                    summary.propertyTables.push_back(std::move(entry));
                }
            }
            if (json.contains("propertyTextures") && json["propertyTextures"].is_array()) {
                summary.propertyTextures = static_cast<uint32_t>(json["propertyTextures"].size());
            }
            if (json.contains("propertyAttributes") && json["propertyAttributes"].is_array()) {
                summary.propertyAttributes = static_cast<uint32_t>(json["propertyAttributes"].size());
            }
        }
    }
    for (const Mesh& mesh : gltf.getMeshes()) {
        for (const Primitive& primitive : mesh.getPrimitives()) {
            if (const std::string* text = findUnknown(primitive, MeshFeatures)) {
                ++summary.meshFeaturePrimitives;
                for (const FeatureIdSet& set : parseFeatureIds(*text, false)) {
                    ++summary.featureIdSets;
                    if (set.source.rfind("attribute", 0) == 0) ++summary.featureIdAttributes;
                    else if (set.source.rfind("texture", 0) == 0) ++summary.featureIdTextures;
                    else ++summary.featureIdImplicit;
                }
            }
            if (findUnknown(primitive, StructuralMetadata) != nullptr) ++summary.metadataPrimitives;
            if (findUnknown(primitive, PrimitiveOutline) != nullptr) ++summary.outlinePrimitives;
        }
    }
    for (const Node& node : gltf.getNodes()) {
        if (const std::string* text = findUnknown(node, InstanceFeatures)) {
            ++summary.instanceFeatureNodes;
            summary.instanceFeatureIdSets += static_cast<uint32_t>(parseFeatureIds(*text, true).size());
        }
    }
    return summary;
}

// ---------------------------------------------------------------------------------------------------------------------
// Microsoft extensions (gltf/extensions/MsftExtensions.h): MSFT_lod chains and the MSFT_packing textures, read from the
// unknown-extension JSON the loader keeps on nodes and materials.
// ---------------------------------------------------------------------------------------------------------------------

ux3d::gltf::msft::LodInfo ux3d::gltf::msft::lod(const GltfObject& object)
{
    LodInfo info;
    const std::string* text = findUnknown(object, Lod);
    if (text == nullptr) {
        return info;
    }
    info.present = true;
    const nlohmann::json json = parseOrNull(*text);
    if (json.is_object() && json.contains("ids") && json["ids"].is_array()) {
        for (const auto& id : json["ids"]) {
            if (id.is_number_integer()) {
                info.ids.push_back(id.get<int>());
            }
        }
    }
    const nlohmann::json extras = object.getExtras().empty() ? nlohmann::json() : parseOrNull(object.getExtras());
    if (extras.is_object() && extras.contains("MSFT_screencoverage") && extras["MSFT_screencoverage"].is_array()) {
        for (const auto& value : extras["MSFT_screencoverage"]) {
            if (value.is_number()) {
                info.screenCoverage.push_back(value.get<double>());
            }
        }
    }
    return info;
}

ux3d::gltf::msft::TextureRef ux3d::gltf::msft::packedOcclusionRoughnessMetallic(const GltfObject& material)
{
    TextureRef ref;
    const std::string* text = findUnknown(material, PackingOrm);
    if (text == nullptr) {
        return ref;
    }
    const nlohmann::json json = parseOrNull(*text);
    if (json.is_object() && json.contains("occlusionRoughnessMetallicTexture")) {
        const nlohmann::json& info = json["occlusionRoughnessMetallicTexture"];
        if (info.is_object() && info.contains("index") && info["index"].is_number_integer() && info["index"].get<int>() >= 0) {
            ref.index = info["index"].get<int>();
            ref.texCoord = info.contains("texCoord") && info["texCoord"].is_number_integer() ? std::max(0, info["texCoord"].get<int>()) : 0;
        }
    }
    return ref;
}

std::vector<std::string> ux3d::gltf::msft::packingSummary(const GltfObject& material)
{
    std::vector<std::string> lines;
    for (const char* name : { PackingOrm, PackingNrm }) {
        const std::string* text = findUnknown(material, name);
        if (text == nullptr) {
            continue;
        }
        const nlohmann::json json = parseOrNull(*text);
        if (!json.is_object()) {
            continue;
        }
        for (const auto& [key, value] : json.items()) {
            if (value.is_object() && value.contains("index") && value["index"].is_number_integer()) {
                lines.push_back(std::string(name) + "." + key + " -> texture " + std::to_string(value["index"].get<int>()));
            }
        }
    }
    return lines;
}
