#pragma once

#include "GltfPropertyPointer.h"
#include "enums/GltfPropertyTypes.h"
#include <QString>

namespace ux3d::editor {
struct BackPathElement {
    GltfPropertyType type;
    QString name;
    GltfPropertyPointer pGltfProperty;
};
}
