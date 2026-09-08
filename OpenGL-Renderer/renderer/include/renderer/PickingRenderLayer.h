// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "ResourceManager.h"
#include "renderer/IRenderLayer.h"

#include <map>

namespace ux3d::renderer::gl {
class GL;
}

namespace ux3d::renderer {
class TransparentPass;

class PickingRenderLayer : public renderer::IRenderLayer {
public:
    PickingRenderLayer(gl::GL* gl);
    RendererScene applyLayer(RendererScene&& scene) override;
    Mesh* getMesh(uint32_t pickingId) const;

    std::function<void()> prePass;
    std::function<void()> postPass;

protected:
    ResourceManager m_pickingResourceManager;
    gl::GL* m_gl = nullptr;

    std::map<uint32_t, Mesh*> m_inverseMapping;
};
}
