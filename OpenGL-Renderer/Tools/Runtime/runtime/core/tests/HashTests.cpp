// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#include <catch2/catch.hpp>

#include "core/Logger.h"
#include "core/TupleHash.h"
#include "core/VectorHash.h"

constexpr auto TestNamespace = "[runtime.core.Hash]";

using namespace ux3d::core;

TEST_CASE("core::hash function", TestNamespace)
{
    // single value
    auto h = hash(123u);

    REQUIRE(h == 17002711862519074878ull);

    // explict seed
    h = hash(123u, detail::g_seed);

    // The hash should still be the same.
    REQUIRE(h == 17002711862519074878ull);
}

TEST_CASE("core::Hash struct", TestNamespace)
{
    using Vec = Vector<int>;

    Vec ints;

    ints.emplace_back(123);

    Hash<Vec> func;

    auto h = func(ints);

    REQUIRE(h == 17002711862519074878ull);

    ints.emplace_back(42);

    h = func(ints);

    REQUIRE(h == 5728033194076326740ull);
}

TEST_CASE("core::Hash struct resolve recursive types", TestNamespace)
{
    using Vec = Vector<int>;

    using VecInVec = Vector<Vec>;

    VecInVec intVecs;

    Vec& vec1 = *intVecs.emplace_back();

    vec1.push_back(123);

    Hash<VecInVec> func;

    auto h = func(intVecs);

    REQUIRE(h == 17002711862519074878ull);

    Vec& vec2 = *intVecs.emplace_back();

    vec2.push_back(42);

    h = func(intVecs);

    REQUIRE(h == 5728033194076326740ull);
}

TEST_CASE("core::Hash tuple", TestNamespace)
{
    using Vec = Vector<int>;

    using Tpl = std::tuple<int, int, Vec>;

    Tpl tpl { 123, 42, {} };

    Hash<Tpl> func;

    auto h = func(tpl);

    REQUIRE(h == 5728033194076326740ull);

    std::get<Vec>(tpl).push_back(1337);

    h = func(tpl);

    REQUIRE(h == hashAll(123, 42, 1337));
}
