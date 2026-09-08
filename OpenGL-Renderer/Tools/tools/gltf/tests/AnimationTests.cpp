// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Gltf.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
constexpr auto TestNamespace = "[tools.gltf.Animation]";

TEST_CASE("Animation: constructor", TestNamespace)
{
    Animation animation(0);

    CHECK(animation.hasName() == false);
    CHECK(animation.getChannels().empty());
    CHECK(animation.getSamplers().empty());
}
}
