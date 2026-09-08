// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Gltf.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
constexpr auto TestNamespace = "[tools.gltf.Texture]";

TEST_CASE("Texture: constructor", TestNamespace)
{
    Texture texture(0);

    CHECK(texture.hasName() == false);
    CHECK(texture.getSampler() == nullptr);
    CHECK(texture.getSource() == nullptr);
}
}
