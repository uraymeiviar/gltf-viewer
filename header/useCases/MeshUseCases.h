#pragma once

#include "BasicUseCases.h"

namespace ux3d::gltf {
class Mesh;
}

namespace ux3d::editor {
struct MeshSelectUseCase : public GenericSelectUseCase<gltf::Mesh> {
};

struct MeshSetNameUseCase : public GenericSetNameUseCase<gltf::Mesh> {
};
}
