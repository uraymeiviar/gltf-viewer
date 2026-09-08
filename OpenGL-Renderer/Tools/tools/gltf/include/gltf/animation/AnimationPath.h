#pragma once

#include "core/String.h"

namespace ux3d::gltf {
enum class AnimationPath : uint32_t {
    Translation,
    Rotation,
    Scale,
    Weights,
    Pointer
};

core::string animationPathToString(AnimationPath _path);
AnimationPath animationPathFromString(const core::string& _string);
}
