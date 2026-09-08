#pragma once

#include <stdint.h>

namespace ux3d::gltf {

struct GlbHeader {
    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t length = 0;
};

struct ChunkHeader {
    uint32_t length = 0;
    uint32_t type = 0;
};
}
