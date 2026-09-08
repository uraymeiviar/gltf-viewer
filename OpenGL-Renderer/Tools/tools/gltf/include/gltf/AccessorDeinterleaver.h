// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "gltf/GltfResult.h"

namespace ux3d::gltf {

class Gltf;
class Accessor;

class AccessorDeinterleaver {
public:
    AccessorDeinterleaver(Gltf& _gltf, Accessor& _accessor);
    GltfResult deinterleave();
    static bool isInterleaved(Gltf& gltf, Accessor& accessor);

private:
    Gltf& m_gltf;
    Accessor& m_accessor;
};
}
