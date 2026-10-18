#pragma once

#include "ClipboardContext.h"
#include "gltf/GltfType.h"
#include <QMimeData>

namespace ux3d::editor {
struct GltfMimeData : public QMimeData {
    gltf::GltfType type = gltf::GltfType::Undefined;
    int64_t id = -1;
    ClipboardContext context;

    Q_OBJECT
};
}
