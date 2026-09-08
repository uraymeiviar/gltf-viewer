#include "gltf/AlphaMode.h"
#include "core/Logger.h"

ux3d::core::string ux3d::gltf::alphaModeToString(AlphaMode _alphaMode)
{
    switch (_alphaMode) {
    case ux3d::gltf::AlphaMode::Opaque:
        return "OPAQUE";
    case ux3d::gltf::AlphaMode::Mask:
        return "MASK";
    case ux3d::gltf::AlphaMode::Blend:
        return "BLEND";
    default:
        break;
    }

    //If no matching AlphaMode was found:
    core::logError("No matching AlphaMode was found.");
    return "";
}

ux3d::gltf::AlphaMode ux3d::gltf::alphaModeFromString(const core::string& _stringAlphaMode)
{
    if (_stringAlphaMode == "OPAQUE") {
        return ux3d::gltf::AlphaMode::Opaque;
    }
    if (_stringAlphaMode == "MASK") {
        return ux3d::gltf::AlphaMode::Mask;
    }
    if (_stringAlphaMode == "BLEND") {
        return ux3d::gltf::AlphaMode::Blend;
    }

    //If no matching AlphaMode was found:
    core::logError("Cannot parse AlphaMode (OPAQUE, MASK, BLEND) from '%s'", _stringAlphaMode.c_str());
    return ux3d::gltf::AlphaMode::Opaque;
}
