#pragma once

#include "BasicUseCases.h"

namespace ux3d::gltf {
class Primitive;
}

namespace ux3d::editor {
struct PrimitiveSelectUseCase : public GenericSelectUseCase<gltf::Primitive> {
};
}
