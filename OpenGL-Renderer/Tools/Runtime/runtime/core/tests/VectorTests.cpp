// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include <catch2/catch.hpp>

#include "core/LinearAllocator.h"
#include "core/Vector.h"

constexpr auto TestNamespace = "[runtime.core.Vector]";

TEST_CASE("Vector late allocation", TestNamespace)
{
    ux3d::core::LinearAllocator arena(1024);
    ux3d::core::Vector<uint32_t> vector(&arena);
    REQUIRE(vector.size() == 0u);

    constexpr size_t capacity = 10u;
    REQUIRE(vector.reserve(capacity, &arena));
    REQUIRE(vector.size() == 0u);
    REQUIRE(vector.capacity() == capacity);
}

void pushAndPopRecursive(ux3d::core::Vector<uint32_t>& _Vector)
{
    if (_Vector.size() == _Vector.capacity()) {
        return;
    }
    size_t size = _Vector.size();
    REQUIRE(_Vector.push_back(static_cast<uint32_t>(size)) != nullptr);

    pushAndPopRecursive(_Vector);

    size_t poppedSize = _Vector.pop_back();
    REQUIRE(size == poppedSize);
}

TEST_CASE("Vector push and pop", TestNamespace)
{
    ux3d::core::LinearAllocator arena(64);
    ux3d::core::Vector<uint32_t> vector(&arena);
    pushAndPopRecursive(vector);
}

TEST_CASE("Vector init list ctor", TestNamespace)
{
    ux3d::core::LinearAllocator arena(64);
    ux3d::core::Vector<uint32_t> vector({ 1, 2, 3, 4 }, &arena);
    REQUIRE(vector.size() == 4);
    REQUIRE(vector[0] == 1);
    REQUIRE(vector[1] == 2);
    REQUIRE(vector[2] == 3);
    REQUIRE(vector[3] == 4);
}

TEST_CASE("Vector init list assignment op", TestNamespace)
{
    ux3d::core::LinearAllocator arena(64);
    ux3d::core::Vector<uint32_t> vector(&arena);
    vector = { 1, 2, 3, 4 };
    REQUIRE(vector.size() == 4);
    REQUIRE(vector[0] == 1);
    REQUIRE(vector[1] == 2);
    REQUIRE(vector[2] == 3);
    REQUIRE(vector[3] == 4);
}

TEST_CASE("Test vector clear", TestNamespace)
{
    ux3d::core::LinearAllocator arena(64);
    ux3d::core::Vector<uint32_t> vector(&arena);

    REQUIRE(vector.push_back(0u) != nullptr);
    REQUIRE(vector.size() == 1u);

    vector.clear();
    REQUIRE(vector.size() == 0u);
}

TEST_CASE("Test vector erase", TestNamespace)
{
    ux3d::core::LinearAllocator arena(64);
    ux3d::core::Vector<uint32_t> vector;

    REQUIRE(vector.reserve(10, &arena));

    for (uint32_t i = 0u; i < vector.capacity(); ++i) {
        REQUIRE(vector.push_back(i) != nullptr);
    }

    auto elemIt = vector.begin() + 5;
    uint32_t elem = *elemIt;
    vector.erase(elemIt);

    auto found = std::find(vector.begin(), vector.end(), elem);
    REQUIRE(found == vector.end());
}

TEST_CASE("Vector.erase_keep_order", TestNamespace)
{
    ux3d::core::LinearAllocator arena(64);
    ux3d::core::Vector<uint32_t> vector;

    REQUIRE(vector.reserve(10, &arena));

    for (uint32_t i = 0u; i < vector.capacity(); ++i) {
        REQUIRE(vector.push_back(i) != nullptr);
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

TEST_CASE("Test vector reserve", TestNamespace)
{
    ux3d::core::LinearAllocator arena1(120);
    ux3d::core::LinearAllocator arena2(80);
    ux3d::core::Vector<uint32_t> vector;

    // initalize with data
    REQUIRE(vector.resizeWithValue(10ull, 0xABCDEFu, &arena1));

    // move to new allocator
    REQUIRE(vector.reserve(20u, &arena2));

    // check if data was moved
    for (const auto& i : vector) {
        REQUIRE(i == 0xABCDEF);
    }

    REQUIRE(vector.capacity() == 20u);
    REQUIRE(vector.size() == 10u);
}

TEST_CASE("Test vector push_back reserve", TestNamespace)
{
    ux3d::core::LinearAllocator arena(200);
    ux3d::core::Vector<uint32_t> vector(&arena);

    for (uint32_t i = 0; i < 10; ++i) {
        REQUIRE(vector.push_back(i) != nullptr);
    }

    for (uint32_t i = 0; i < 10; ++i) {
        REQUIRE(vector[i] == i);
    }

    REQUIRE(vector.capacity() >= vector.size());
    REQUIRE(vector.capacity() >= 10u);
    REQUIRE(vector.size() == 10u);
}

//struct C {
//	explicit C(int id) : id(id) {}
//	C(const C& rhs) : id(rhs.id), copied(true) {}
//	C(C&& rhs) noexcept : id(rhs.id), moved(true) {}
//
//	int id;
//	bool moved = false;
//	bool copied = false;
//};
//
//TEST_CASE("Collect from iterator", TestNamespace)
//{
//	using namespace ux3d::core;
//
//	HeapVector<C> v0;
//	v0.emplace_back(1);
//	v0.emplace_back(2);
//	v0.emplace_back(3);
//
//	HeapVector<C> v1 = collect(std::move(v0.begin()), v0.end());
//
//	REQUIRE(v1[0].id == 1);
//	REQUIRE(!v1[0].copied);
//	REQUIRE(v1[0].moved);
//
//	REQUIRE(v1[1].id == 2);
//	REQUIRE(!v1[1].copied);
//	REQUIRE(v1[1].moved);
//
//	REQUIRE(v1[2].id == 3);
//	REQUIRE(!v1[2].copied);
//	REQUIRE(v1[2].moved);
//}

TEST_CASE("Initializer list assign", TestNamespace)
{
    using namespace ux3d::core;

    Vector<int> v;

    REQUIRE(v.assign({ 1, 2, 3, 4, 5 }));

    REQUIRE(v.size() == 5);
    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 2);
    REQUIRE(v[2] == 3);
    REQUIRE(v[3] == 4);
    REQUIRE(v[4] == 5);
}

TEST_CASE("Copy, move, assign from uninitialized vector", TestNamespace)
{
    ux3d::core::Vector<int32_t> vector2;

    SECTION("assignment")
    {
        ux3d::core::Vector<int32_t> vector1;
        vector1 = vector2;
        CHECK(vector1.getAllocator() == vector2.getAllocator());
    }

    SECTION("move assign")
    {
        ux3d::core::Vector<int32_t> vector1;
        vector1 = std::move(vector2);
        CHECK(vector1.getAllocator() != nullptr);
    }

    SECTION("copy construct")
    {
        ux3d::core::Vector<int32_t> vector1(vector2);
        CHECK(vector1.getAllocator() == vector2.getAllocator());
    }

    SECTION("move construct")
    {
        ux3d::core::Vector<int32_t> vector1(std::move(vector2));
        CHECK(vector1.getAllocator() != nullptr);
    }
}
