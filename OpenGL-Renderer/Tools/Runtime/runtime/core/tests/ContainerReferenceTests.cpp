// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include <catch2/catch.hpp>

#include "core/Logger.h"
#include "core/Vector.h"

constexpr auto TestNamespace = "[runtime.core.ContainerReference]";

TEST_CASE("stability over vector reallocation", TestNamespace)
{
    ux3d::core::Vector<int> v;

    ux3d::core::VectorRef<int> ref_0 = v.emplace_back_ref(0);

    // Here, the vector reallocates because of insufficient capacity.
    ux3d::core::VectorRef<int> ref_1 = v.emplace_back_ref(1);
    (void)ref_1;

    // The reference should still be the same.
    REQUIRE(ref_0 == v.refAt(0));
}

TEST_CASE("assignment", TestNamespace)
{
    ux3d::core::Vector<int> v;

    ux3d::core::VectorRef<int> ref;

    ref = v.emplace_back_ref(0);
    ref = v.emplace_back_ref(1);

    REQUIRE(ref == v.refAt(1));
}

TEST_CASE("container and elements are const", TestNamespace)
{
    ux3d::core::Vector<int> v;

    ux3d::core::VectorRef<int> ref;
    ref = v.emplace_back_ref(0);

    ux3d::core::VectorConstRef<int> c_ref;
    c_ref = ref;

    REQUIRE((c_ref == v.refAt(0)));
}

TEST_CASE("Iterator interface", TestNamespace)
{
    ux3d::core::Vector<int> v(ux3d::core::HeapAllocator::defaultInstance());
    REQUIRE(v.push_back('a'));
    REQUIRE(v.push_back('b'));
    REQUIRE(v.push_back('c'));

    ux3d::core::VectorConstRef<int> ref = v.begin_ref_const();
    REQUIRE(*ref == 'a');
    REQUIRE(*++ref == 'b');
    REQUIRE(*++ref == 'c');
    REQUIRE(++ref == v.end_ref_const());

    ux3d::core::VectorRef<int> ref_mut = v.begin_ref();
    REQUIRE(*ref_mut == 'a');
    REQUIRE(ref_mut.container().push_back('d'));
    REQUIRE(*++ref_mut == 'b');
    REQUIRE(*++ref_mut == 'c');
    REQUIRE(*++ref_mut == 'd');
    REQUIRE(++ref_mut == v.end_ref());
}

TEST_CASE("Same container", TestNamespace)
{
    ux3d::core::Vector<int> v1(ux3d::core::HeapAllocator::defaultInstance());
    ux3d::core::Vector<int> v2(ux3d::core::HeapAllocator::defaultInstance());

    REQUIRE(v1.begin_ref_const().has_same_container(v1.end_ref_const()));
    REQUIRE(!v2.begin_ref_const().has_same_container(v1.end_ref_const()));
    REQUIRE(!v1.begin_ref_const().has_same_container(v2.end_ref_const()));
}
