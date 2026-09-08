// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/EnumOps.h"
#include <cstdint>
#include <string>

namespace ux3d::gltf {
enum class AnimationType : uint32_t {
    Translation,
    Rotation
};

std::string AnimationTypeToString(AnimationType _type);
AnimationType stringToAnimationType(const std::string& _text);

}
