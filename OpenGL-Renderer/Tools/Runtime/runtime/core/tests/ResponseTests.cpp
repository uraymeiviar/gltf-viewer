// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"

#include "core/Response.h"
#include "core/Vector.h"

using namespace ux3d;

enum class AppError {
    IsZero
};

core::Response<int, AppError> divide(int nom, int den)
{
    if (den == 0) {
        return AppError::IsZero;
    }
    return nom / den;
}

core::Response<int, AppError> computation(int a, int b)
{
    const auto x = divide(a, b);
    if (!x) {
        return x;
    }

    return divide(*x, b);
}

core::Response<std::tuple<int, int>, AppError> f()
{
    return std::tuple { 1, 2 };
}

core::BoolResponse<AppError> check(int n)
{
    if (n == 0) {
        return AppError::IsZero;
    } else {
        return core::SuccessResponse {};
    }
}

TEST_CASE("Basic usage success", "[runtime.core.Response]")
{
    auto x = divide(6, 3);
    REQUIRE(x);
    REQUIRE(*x == 2);
}

TEST_CASE("Basic usage error", "[runtime.core.Response]")
{
    auto x = divide(6, 0);
    REQUIRE(!x);
    REQUIRE(x.error() == AppError::IsZero);
}

TEST_CASE("and_then success", "[runtime.core.Response]")
{
    auto x = divide(24, 3)
                 .andThen([](int x) { return divide(x, 2); });

    REQUIRE(x);
    REQUIRE(*x == 4);
}

TEST_CASE("and_then first error", "[runtime.core.Response]")
{
    auto x = divide(6, 0)
                 .andThen([](int x) { return divide(x, 2); });

    REQUIRE(!x);
    REQUIRE(x.error() == AppError::IsZero);
}

TEST_CASE("and_then second error", "[runtime.core.Response]")
{
    auto x = divide(6, 3)
                 .andThen([](int x) { return divide(x, 0); });

    REQUIRE(!x);
    REQUIRE(x.error() == AppError::IsZero);
}

TEST_CASE("or_else success", "[runtime.core.Response]")
{
    auto x = divide(24, 0)
                 .orElse(divide(24, 2));

    REQUIRE(x);
    REQUIRE(*x == 12);
}

TEST_CASE("or_else error", "[runtime.core.Response]")
{
    auto x = divide(24, 0)
                 .orElse(divide(12, 0));

    REQUIRE(!x);
    REQUIRE(x.error() == AppError::IsZero);
}

TEST_CASE("or_default success", "[runtime.core.Response]")
{
    int x = divide(24, 2)
                .orDefault(-1);

    REQUIRE(x == 12);
}

TEST_CASE("or_default error", "[runtime.core.Response]")
{
    int x = divide(24, 0)
                .orDefault(-1);

    REQUIRE(x == -1);
}

TEST_CASE("Usage success", "[runtime.core.Response]")
{
    auto x = computation(16, 2);
    REQUIRE(x);
    REQUIRE(*x == 4);
}

TEST_CASE("Usage error", "[runtime.core.Response]")
{
    auto x = computation(16, 0);
    REQUIRE(!x);
    REQUIRE(x.error() == AppError::IsZero);
}

struct X {
    int field;

    X(int x)
        : field { x }
    {
    }

    X(const X&) = default;

    X(X&& x)
        : field { x.field }
    {
        x.field = -1;
    }
};

TEST_CASE("Unwrap response", "[runtime.core.Response]")
{
    core::Response<X, AppError> r { X { 7 } };

    REQUIRE(r);
    REQUIRE(r->field == 7);

    X x { r.unwrap() };

    REQUIRE(x.field == 7);
    REQUIRE(r->field == -1);
}

TEST_CASE("Unpacking", "[runtime.core.Response]")
{
    auto [x, y] = *f();
    REQUIRE(x == 1);
    REQUIRE(y == 2);
}

TEST_CASE("Bool response", "[runtime.core.Response]")
{
    auto const a = check(1);
    REQUIRE(a);

    auto const b = check(0);
    REQUIRE(!b);
}
