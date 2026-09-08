// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <cstdint>

namespace ux3d::gltf {
enum class MaterialType : uint32_t {
    PbrMetallicRoughness,
    PbrSpecularGlossiness,
    Clearcoat,
    Unlit,
    Specular,
    Transmission,
    Volume
};
}
