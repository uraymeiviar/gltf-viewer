// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#include "renderer/passes/FullscreenCopyPass.h"

using namespace ux3d;
using namespace ux3d::renderer;

ux3d::renderer::FullscreenCopyPass::FullscreenCopyPass()
    : RenderPass(RenderPassType::FullscreenCopy, "FullscreenCopy")
{
}
