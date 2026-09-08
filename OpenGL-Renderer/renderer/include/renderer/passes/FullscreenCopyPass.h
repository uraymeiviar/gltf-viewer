// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "renderer/RenderPass.h"

namespace ux3d::renderer {
class FullscreenCopyPass : public RenderPass {
public:
    FullscreenCopyPass();
    virtual ~FullscreenCopyPass() = default;
};
}
