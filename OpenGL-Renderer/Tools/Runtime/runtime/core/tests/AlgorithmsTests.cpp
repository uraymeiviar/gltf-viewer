// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "core/Algorithms.h"
#include "core/LinkedList.h"
#include "core/Vector.h"
#include <catch2/catch.hpp>

namespace ux3d::core::tests {
constexpr auto TestNamespace = "[runtime.core.Algorithms]";

enum class TestBits : uint32_t {
    None = 0,
    One = 1u << 0u,
    Two = 1u << 1u,
    Three = 1u << 2u,
    Four = 1u << 3u
};

using TestEnum = Flag<TestBits>;

TEST_CASE("Algorithms: hasFlag", TestNamespace)
{
    TestEnum testEnum;

    SECTION("initially no flags")
    {
        CHECK_FALSE(hasFlag(testEnum, TestBits::One));
        CHECK_FALSE(hasFlag(testEnum, TestBits::Two));
        CHECK_FALSE(hasFlag(testEnum, TestBits::Three));
        CHECK_FALSE(hasFlag(testEnum, TestBits::Four));
    }

    SECTION("set one flag")
    {
        testEnum = TestBits::Two;

        CHECK_FALSE(hasFlag(testEnum, TestBits::One));
        CHECK(hasFlag(testEnum, TestBits::Two));
        CHECK_FALSE(hasFlag(testEnum, TestBits::Three));
        CHECK_FALSE(hasFlag(testEnum, TestBits::Four));
    }

    SECTION("set two flags")
    {
        testEnum = TestBits::Two;
        testEnum |= TestBits::Three;

        CHECK_FALSE(hasFlag(testEnum, TestBits::One));
        CHECK(hasFlag(testEnum, TestBits::Two));
        CHECK(hasFlag(testEnum, TestBits::Three));
        CHECK_FALSE(hasFlag(testEnum, TestBits::Four));
    }
}

TEMPLATE_TEST_CASE("Algorithms: contains", TestNamespace, core::Vector<int32_t>, core::LinkedList<int32_t>)
{
    TestType list;

    SECTION("empty list doesn't contain anything")
    {
        CHECK_FALSE(core::contains(list, 0));
        CHECK_FALSE(core::contains(list, 5));
    }
    SECTION("list with some elements contains them")
    {
        REQUIRE(list.emplace_back(0) != nullptr);
        REQUIRE(list.emplace_back(-5) != nullptr);

        CHECK(core::contains(list, 0));
        CHECK(core::contains(list, -5));
        CHECK_FALSE(core::contains(list, 5));
    }
}

TEST_CASE("Algorithms: eqaul", TestNamespace)
{
    SECTION("equal vectors")
    {
        core::Vector<uint32_t> a { 1, 2, 3 };
        CHECK(core::equal(a, { 1, 2, 3 }));
    }
    SECTION("non-equal vectors")
    {
        core::Vector<uint32_t> a { 1, 2, 3 };
        CHECK_FALSE(core::equal(a, { 3, 2, 1 }));
    }
}
}
