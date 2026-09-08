#pragma once

#include <cstdint>

#include <QEvent>

namespace ux3d::editor {
enum class UserEvent : uint32_t {
    Gizmo = QEvent::User
};
}
