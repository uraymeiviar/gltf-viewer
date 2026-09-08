// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include <catch2/catch.hpp>

#include "core/StaticVector.h"

constexpr auto TestNamespace = "[runtime.core.StaticVector]";

template <size_t N>
void pushAndPopRecursive(ux3d::core::StaticVector<uint32_t, N>& _Vector)
{
    if (_Vector.size() == _Vector.capacity()) {
        return;
    }
    size_t size = _Vector.size();
    _Vector.push_back(static_cast<uint32_t>(size));

    pushAndPopRecursive(_Vector);

    size_t poppedSize = _Vector.pop_back();
    REQUIRE(size == poppedSize);
}

TEST_CASE("StaticVector push and pop", TestNamespace)
{
    ux3d::core::StaticVector<uint32_t, 10> vector;
    pushAndPopRecursive(vector);
}

TEST_CASE("StaticVector vector clear", TestNamespace)
{
    ux3d::core::StaticVector<uint32_t, 10> vector;

    vector.push_back(0u);
    REQUIRE(vector.size() == 1u);

    vector.clear();
    REQUIRE(vector.size() == 0u);
}

TEST_CASE("StaticVector vector erase", TestNamespace)
{
    ux3d::core::StaticVector<uint32_t, 10> vector;

    for (uint32_t i = 0u; i < vector.capacity(); ++i) {
        vector.push_back(i);
    }

    auto elemIt = vector.begin() + 5;
    uint32_t elem = *elemIt;
    vector.erase(elemIt);

    auto found = std::find(vector.begin(), vector.end(), elem);
    REQUIRE(found == vector.end());
}

TEST_CASE("StaticVector.erase_keep_order", TestNamespace)
{
    ux3d::core::StaticVector<uint32_t, 10> vector;

    for (uint32_t i = 0u; i < vector.capacity(); ++i) {
        vector.push_back(i);
    }

    REQUIRE(vector.size() == 10);

    auto elemIt = vector.begin() + 5;
    uint32_t elem = *elemIt;
    vector.erase_keep_order(elemIt);

    auto found = std::find(vector.begin(), vector.end(), elem);
    REQUIRE(vector.size() == 9);
    REQUIRE(found == vector.end());

    SECTION("order is preserved")
    {
        uint32_t last = 0;
        for (auto i : vector) {
            REQUIRE(i >= last);
            last = i;
        }
    }
}

TEST_CASE("Test StaticVector push_back reserve", TestNamespace)
{
    ux3d::core::StaticVector<uint32_t, 10> vector;

    for (uint32_t i = 0; i < 10; ++i) {
        vector.push_back(i);
    }

    for (uint32_t i = 0; i < 10; ++i) {
        REQUIRE(vector[i] == i);
    }

    REQUIRE(vector.capacity() >= vector.size());
    REQUIRE(vector.capacity() >= 10u);
    REQUIRE(vector.size() == 10u);
}

TEST_CASE("StaticVector Initializer list ctor", TestNamespace)
{
    using namespace ux3d::core;

    auto v = makeStaticVector({ 1, 2, 3, 4, 5 });

    REQUIRE(v.size() == 5);
    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 2);
    REQUIRE(v[2] == 3);
    REQUIRE(v[3] == 4);
    REQUIRE(v[4] == 5);
}

TEST_CASE("StaticVector assign", TestNamespace)
{
    using namespace ux3d::core;

    auto a = makeStaticVector({ 1, 2, 3, 4, 5 });

    StaticVector<int, 5> b;

    b.assign(a);

    REQUIRE(b.size() == 5);
    REQUIRE(b[0] == 1);
    REQUIRE(b[1] == 2);
    REQUIRE(b[2] == 3);
    REQUIRE(b[3] == 4);
    REQUIRE(b[4] == 5);
}

TEST_CASE("StaticVector copy construct", TestNamespace)
{
    using namespace ux3d::core;

    auto a = makeStaticVector({ 1, 2, 3, 4, 5 });

    StaticVector<int, 5> b(a);

    REQUIRE(b.size() == 5);
    REQUIRE(b[0] == 1);
    REQUIRE(b[1] == 2);
    REQUIRE(b[2] == 3);
    REQUIRE(b[3] == 4);
    REQUIRE(b[4] == 5);
}
