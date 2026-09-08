#pragma once

#include "core/String.h"

namespace ux3d::gltf {
enum class AnimationInterpolation : uint32_t {
    Linear,
    Step,
    CubicSpline
};

core::string animationInterpolationToString(AnimationInterpolation _interpolation);
AnimationInterpolation animationInterpolationFromString(const core::string& _string);
}
