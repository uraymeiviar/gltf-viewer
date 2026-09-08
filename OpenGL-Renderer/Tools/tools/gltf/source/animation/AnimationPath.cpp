#include "gltf/animation/AnimationPath.h"
#include "core/Logger.h"

ux3d::core::string ux3d::gltf::animationPathToString(AnimationPath _path)
{
    switch (_path) {
    case AnimationPath::Translation:
        return "translation";
    case AnimationPath::Rotation:
        return "rotation";
    case AnimationPath::Scale:
        return "scale";
    case AnimationPath::Weights:
        return "weights";
    case AnimationPath::Pointer:
        return "pointer";
    }

    core::logError("Invalid value %u for AnimationPath", static_cast<uint32_t>(_path));
    return "";
}

ux3d::gltf::AnimationPath ux3d::gltf::animationPathFromString(const core::string& _string)
{
    if (_string == "translation") {
        return AnimationPath::Translation;
    }
    if (_string == "rotation") {
        return AnimationPath::Rotation;
    }
    if (_string == "scale") {
        return AnimationPath::Scale;
    }
    if (_string == "weights") {
        return AnimationPath::Weights;
    }
    if (_string == "pointer") { // KHR_animation_pointer
        return AnimationPath::Pointer;
    }

    core::logError("Cannot parse AnimationPath from '%s'", _string.c_str());
    return AnimationPath();
}
