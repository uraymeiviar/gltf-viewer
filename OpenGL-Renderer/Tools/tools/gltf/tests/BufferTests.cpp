// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Gltf.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
constexpr auto TestNamespace = "[tools.gltf.Buffer]";

TEST_CASE("Buffer: constructor", TestNamespace)
{
    Buffer buffer(0);

    CHECK(buffer.getUri().empty());

    core::MemoryRange<const uint8_t> bufferData;
    buffer.getData(bufferData);

    CHECK(bufferData.empty());
    CHECK(buffer.getByteLength() == 0u);
}
}
