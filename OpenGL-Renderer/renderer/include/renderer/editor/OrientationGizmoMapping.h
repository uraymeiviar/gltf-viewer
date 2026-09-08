//
//  OrientationGizmoMapping.h
//  Gestaltor
//
//  Created by Moritz Willy Becher on 28.04.21.
//

#pragma once

#include "renderer/Mapping.h"

#include <tuple>

namespace ux3d::renderer {
class Material;
class Mesh;
class EditorRenderLayer;

enum class OrientationGizmoPiece {
    Top = 0u,
    Bottom,
    Right,
    Left,
    Front,
    Back,
    Num,
    Invalid
};

enum class OrientationCubeInteractionMode {
    Default = 0u,
    Hover,
    Selected
};

using OrientationGizmoMappingKey = std::tuple<OrientationGizmoPiece, OrientationCubeInteractionMode*>;
using OrientationGizmoMappingValue = std::tuple<Mesh*, Material*>;
class OrientationGizmoMapping : public Mapping<OrientationGizmoMappingKey, OrientationGizmoMappingValue> {
public:
    OrientationGizmoMapping(EditorRenderLayer& renderLayer);

protected:
    bool onUpdateMapping(OrientationGizmoMappingKey key, OrientationGizmoMappingValue value) final;
    OrientationGizmoMappingValue onCreateMapping(OrientationGizmoMappingKey key) final;
    void onDestroyMapping(OrientationGizmoMappingKey key, OrientationGizmoMappingValue value) final;

private:
    EditorRenderLayer& m_renderLayer;
};
}
