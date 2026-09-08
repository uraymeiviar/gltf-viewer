// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Gltf.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
constexpr auto TestNamespace = "[tools.gltf.Skin]";

TEST_CASE("Skin: constructor", TestNamespace)
{
    Skin skin(0);

    CHECK(skin.hasName() == false);
    CHECK(skin.getInverseBindMatricesAccessor() == nullptr);
    CHECK(skin.getSkeletonRoot() == nullptr);
    CHECK(skin.getJoints().empty());
}
}
