#include "gltf/Extensions.h"
#include "core/Algorithms.h"
#include "core/Logger.h"

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
    if (_extension == Extension::Audio || _extension == Extension::PunctualLights || _extension == Extension::MaterialsVariants || _extension == Extension::XMP || _extension == Extension::PbrSpecularGlossinessMaterial || _extension == Extension::UnlitMaterial || _extension == Extension::Clearcoat || _extension == Extension::Specular || _extension == Extension::Sheen || _extension == Extension::Ior || _extension == Extension::Transmission || _extension == Extension::Volume || _extension == Extension::EmissiveStrength) {
        return false;
    }

    if (_extension == Extension::DracoMeshCompression || _extension == Extension::MeshQuantization || _extension == Extension::TextureTransform || _extension == Extension::TextureBasisU) {
        return true;
    }

    return true;
}
