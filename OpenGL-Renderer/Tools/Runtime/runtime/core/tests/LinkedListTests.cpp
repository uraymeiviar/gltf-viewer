// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include <catch2/catch.hpp>

#include "core/LinkedList.h"
#include "core/Logger.h"

class C {
public:
    explicit C(int d)
        : m_Int(d)
    {
    }
    C(const C& rhs) = default;

    bool operator==(const C& rhs) const
    {
        return m_Int == rhs.m_Int;
    }

    bool operator==(const C&& rhs) const
    {
        return m_Int == rhs.m_Int;
    }

private:
    int m_Int;
};

constexpr auto TestNamespace = "[runtime.core.LinkedList]";

TEST_CASE("LinkedList emplace back and erase", TestNamespace)
{
    ux3d::core::LinkedList<int> list;

    int* item = list.emplace_back(5);
    REQUIRE(item);
    REQUIRE(*item == 5);
    REQUIRE(*list.begin() == 5);
    item = list.emplace_back(6);
    REQUIRE(*item == 6);
    REQUIRE(*(list.begin() + 1) == 6);
    item = list.emplace_back(7);
    REQUIRE(*item == 7);
    REQUIRE(*(list.begin() + 2) == 7);
    REQUIRE(list.emplace_back(8));
    REQUIRE(list.size() == 4);

    auto it = list.erase(list.begin() + 2);
    REQUIRE(list.size() == 3);
    REQUIRE(*it == 8);
    it = list.erase(it);
    REQUIRE(list.size() == 2);
    REQUIRE(it == list.end());
    it = list.erase(list.begin());
    REQUIRE(list.size() == 1);
    REQUIRE(it == list.begin());
    it = list.erase(list.begin());
    REQUIRE(list.empty());
    REQUIRE((it == list.begin() && it == nullptr));
}

TEST_CASE("front and back", TestNamespace)
{
    ux3d::core::LinkedList<int> list;
    REQUIRE(list.emplace_back(1));
    REQUIRE(list.emplace_back(2));
    REQUIRE(list.emplace_back(3));
    CHECK(list.front() == 1);
    CHECK(list.back() == 3);
}

TEST_CASE("concat", TestNamespace)
{
    using namespace ux3d::core;
    LinkedList<C> list1, list2;

    REQUIRE(list1.emplace_back(1));
    REQUIRE(list1.emplace_back(2));

    REQUIRE(list2.emplace_back(3));
    REQUIRE(list2.emplace_back(4));

    REQUIRE(list1.concat(list2));

    REQUIRE(list1.size() == 4);
    REQUIRE(list2.size() == 2);

    auto iter1 = list1.begin();
    auto iter2 = list2.begin();

    REQUIRE(*iter1 == C(1));
    REQUIRE(*++iter1 == C(2));
    REQUIRE(*++iter1 == C(3));
    REQUIRE(*++iter1 == C(4));
    REQUIRE(++iter1 == list1.end());

    REQUIRE(*iter2 == C(3));
    REQUIRE(*++iter2 == C(4));
    REQUIRE(++iter2 == list2.end());

    REQUIRE((list1.begin() + 2) != list2.begin());
}

TEST_CASE("Concat list with moving ownership", TestNamespace)
{
    using namespace ux3d::core;
    LinkedList<int> l0, l1;

    REQUIRE(l0.emplace_back(0));
    REQUIRE(l0.emplace_back(1));
    REQUIRE(l0.emplace_back(2));
    REQUIRE(l1.emplace_back(3));
    REQUIRE(l1.emplace_back(4));
    REQUIRE(l1.emplace_back(5));

    l0.take(l1);

    REQUIRE(6 == l0.size());
    REQUIRE(l1.empty());

    auto iter = l0.begin();
    REQUIRE(0 == *iter);
    REQUIRE(1 == *++iter);
    REQUIRE(2 == *++iter);
    REQUIRE(3 == *++iter);
    REQUIRE(4 == *++iter);
    REQUIRE(5 == *++iter);
}

TEST_CASE("Insert into list", TestNamespace)
{
    using namespace ux3d::core;
    LinkedList<int> l0;

    REQUIRE(l0.emplace_back(1));
    REQUIRE(l0.emplace_back(2));
    REQUIRE(l0.emplace_back(3));
    REQUIRE(l0.emplace_back(4));
    REQUIRE(l0.emplace_back(5));

    REQUIRE(l0.emplace(l0.begin() + 2, 10));
    REQUIRE(6 == l0.size());
    REQUIRE(10 == l0.at(2));
    REQUIRE(2 == l0.at(1));
    REQUIRE(3 == l0.at(3));

    REQUIRE(l0.emplace(l0.begin(), 11));
    REQUIRE(7 == l0.size());
    REQUIRE(11 == l0.at(0));
    REQUIRE(1 == l0.at(1));

    REQUIRE(l0.emplace(l0.end(), 12));
    REQUIRE(8 == l0.size());
    REQUIRE(12 == l0.at(7));
}
