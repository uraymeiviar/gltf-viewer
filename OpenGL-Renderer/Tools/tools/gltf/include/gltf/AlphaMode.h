// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once
#include "core/String.h"
#include <cstdint>

namespace ux3d::gltf {
enum class AlphaMode : uint32_t {
    Opaque,
    Mask,
    Blend
};

core::string alphaModeToString(AlphaMode _alphaMode);
AlphaMode alphaModeFromString(const core::string& _stringAlphaMode);
}
