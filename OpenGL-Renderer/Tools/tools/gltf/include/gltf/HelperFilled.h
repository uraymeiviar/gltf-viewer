// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "gltf/GltfResult.h"

namespace ux3d::gltf {
class HelperFilled {
private:
    bool m_Filled = true;

public:
    bool getFilled() const
    {
        return m_Filled;
    }

    GltfResult setFilled(bool _Filled)
    {
        m_Filled = _Filled;

        return GltfResult::Success;
    }
};
}
