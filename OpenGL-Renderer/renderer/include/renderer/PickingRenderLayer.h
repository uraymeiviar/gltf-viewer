// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "ResourceManager.h"
#include "renderer/IRenderLayer.h"
#include "renderer/Material.h"

#include <map>
#include <memory>
#include <vector>

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

    // Picking materials are kept in a pool and refreshed by copy-assignment each
    // call instead of being created and destroyed per drawable per frame (P4.6).
    size_t getMaterialPoolSize() const { return m_materialPool.size(); }
    uint64_t getMaterialsCreated() const { return m_materialsCreated; }
    uint64_t getMaterialsReused() const { return m_materialsReused; }

protected:
    ResourceManager m_pickingResourceManager;
    gl::GL* m_gl = nullptr;

    // Index = picking id; ids are assigned consecutively from 1 per applyLayer call.
    std::vector<Mesh*> m_inverseMapping;
    std::vector<std::unique_ptr<Material>> m_materialPool;
    uint64_t m_materialsCreated = 0;
    uint64_t m_materialsReused = 0;
};
}
