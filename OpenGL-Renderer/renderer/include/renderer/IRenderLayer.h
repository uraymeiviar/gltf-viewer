// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "renderer/RendererScene.h"

#include <utility>

namespace ux3d::renderer {
// IRenderLayer is the interface that render layers must fulfill. RenderLayers are the suggested way of interfacing with the
// renderer. Renderlayers allow a configuration of the renderer scene in a way similar to a "Onion". E.g. we can have a low level
// gltf layer that plainly generates renderer objects for the gltf objects. Above that, a editor layer could add gizmos depending on the content of the gltf layer. Layers are therefore organized like an onion, where higher layers can extend and modify the output of lower layers.
class IRenderLayer {
public:
    virtual ~IRenderLayer() = default;
    virtual RendererScene applyLayer(RendererScene&& scene)
    {
        return std::move(scene);
    }
};
}
