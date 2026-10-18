#pragma once

#include "renderer/editor/GizmoMapping.h"
#include <QEvent>

namespace ux3d::editor {
class GizmoEvent : public QEvent {
public:
    GizmoEvent(renderer::GizmoState _state);
    renderer::GizmoState getState() const;
    gltf::Node* getNode() const;

private:
    renderer::GizmoState m_state;
};
}
