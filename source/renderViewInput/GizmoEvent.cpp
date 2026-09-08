#include "renderViewInput/GizmoEvent.h"
#include "enums/UserEvents.h"

using namespace ux3d;
using namespace ux3d::editor;

GizmoEvent::GizmoEvent(renderer::GizmoState _state)
    : QEvent(static_cast<QEvent::Type>(UserEvent::Gizmo))
    , m_state(_state)
{
}

renderer::GizmoState GizmoEvent::getState() const
{
    return m_state;
}
