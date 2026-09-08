#pragma once

#include "gltf/GltfResult.h"

namespace ux3d::gltf {
class Gltf;
class Primitive;

class DracoDecoder {
public:
    DracoDecoder(Gltf& gltf);
    GltfResult decodePrimitive(Primitive& primitive);

private:
    Gltf& m_gltf;
};
}
