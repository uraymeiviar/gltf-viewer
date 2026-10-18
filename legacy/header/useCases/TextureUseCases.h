#pragma once

#include "BasicUseCases.h"

namespace ux3d::gltf {
class Texture;
class Image;
class Sampler;
}

namespace ux3d::editor {
struct TextureSelectUseCase : public GenericSelectUseCase<gltf::Texture> {
};

struct TextureSetNameUseCase : public GenericSetNameUseCase<gltf::Texture> {
};
}
