#include "gltf/animation/AnimationInterpolation.h"
#include "core/Logger.h"

ux3d::core::string ux3d::gltf::animationInterpolationToString(AnimationInterpolation _interpolation)
{
    switch (_interpolation) {
    case AnimationInterpolation::Linear:
        return "LINEAR";
    case AnimationInterpolation::Step:
        return "STEP";
    case AnimationInterpolation::CubicSpline:
        return "CUBICSPLINE";
    }

    core::logError("Invalid value %u for AnimationInterpolation", static_cast<uint32_t>(_interpolation));
    return "";
}

ux3d::gltf::AnimationInterpolation ux3d::gltf::animationInterpolationFromString(const core::string& _string)
{
    if (_string == "LINEAR") {
        return AnimationInterpolation::Linear;
    }
    if (_string == "STEP") {
        return AnimationInterpolation::Step;
    }
    if (_string == "CUBICSPLINE") {
        return AnimationInterpolation::CubicSpline;
    }

    core::logError("Cannot parse AnimationInterpolation from '%s'", _string.c_str());
    return AnimationInterpolation();
}
