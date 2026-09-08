// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "gltf/GltfResult.h"

namespace ux3d::gltf {
class HelperVisible {
private:
    bool m_Visible = true;

public:
    bool getVisible() const
    {
        return m_Visible;
    }

    GltfResult setVisible(bool _Visible)
    {
        m_Visible = _Visible;

        return GltfResult::Success;
    }
};
}
