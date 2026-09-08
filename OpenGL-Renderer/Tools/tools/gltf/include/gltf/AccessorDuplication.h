// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "gltf/GltfResult.h"

namespace ux3d::gltf {
class Gltf;
class Accessor;

class AccessorDuplication {
public:
    AccessorDuplication(Gltf& _gltf);
    bool canDuplicate(const Accessor& _accessor);
    Accessor* duplicate(const Accessor& _accessor, bool _isIndicesAccessor);

private:
    Gltf& m_gltf;
};
}
