// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Gltf.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
constexpr auto TestNamespace = "[tools.gltf.Sampler]";

TEST_CASE("Sampler: constructor", TestNamespace)
{
    Gltf gltf;
    Sampler sampler(0);

    CHECK(sampler.getWrapSOrDefault() == TextureWrappingMode::Repeat);
    CHECK(sampler.getWrapTOrDefault() == TextureWrappingMode::Repeat);
}
}
