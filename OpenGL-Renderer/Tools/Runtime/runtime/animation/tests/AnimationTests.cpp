// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴

#include <catch2/catch.hpp>

#include "core/HeapAllocator.h"

#include "animation/Animation.h"
#include "animation/Channel.h"
#include "animation/Sampler.h"

#include "AnimationTarget.h"

TEST_CASE("Animation", "[runtime.animation.Animation]")
{
    SECTION("Simple animation")
    {
        float reference = 0.0f;
        std::function<void(float)> update = [&reference](float value) {
            REQUIRE(ux3d::math::epsilonEqual(value, reference, 0.01f));
        };
        AnimationTarget<float, std::function<void(float)>> target(update);
        ux3d::animation::Channel<float> channel(target, &ux3d::animation::linear<float>);
        channel.addKeyframe(0.0f, 0.0f);
        channel.addKeyframe(1.0f, 1.0f);

        ux3d::animation::Animation anim(2.0);
        anim.channels.emplace_back(channel);

        reference = 0.0f;
        anim.updateChannels(0.0f);

        reference = 0.5f;
        anim.updateChannels(0.5f);

        reference = 1.0f;
        anim.updateChannels(1.0f);
    }
}

TEST_CASE("Channel", "[runtime.animation.Channel]")
{
    SECTION("Empty keyframes")
    {
        std::function<void(float)> neverCalled = [](float) {
            REQUIRE(false);
        };
        AnimationTarget<float, std::function<void(float)>> target(neverCalled);
        ux3d::animation::Channel<float> channel(target, &ux3d::animation::linear<float>);
        channel.update(0.0f);
    }

    SECTION("one keyframe")
    {
        std::function<void(float)> one = [](float value) {
            REQUIRE(value == 1.0f);
        };
        AnimationTarget<float, std::function<void(float)>> target(one);
        ux3d::animation::Channel<float> channel(target, &ux3d::animation::linear<float>);
        channel.addKeyframe(0.0f, 1.0f);
        channel.update(1.0f);
        channel.update(-1.0f);
    }

    SECTION("two keyframes")
    {
        std::function<void(float)> halfway = [](float value) {
            REQUIRE(value == 0.5f);
        };
        AnimationTarget<float, std::function<void(float)>> target(halfway);
        ux3d::animation::Channel<float> channel(target, &ux3d::animation::linear<float>);
        channel.addKeyframe(0.0f, 0.0f);
        channel.addKeyframe(1.0f, 1.0f);
        channel.update(0.5f);
    }
}

template <class T>
bool test_step(const T& _x, const T& _y, const float _interpolant)
{
    T output = ux3d::animation::step(_x, T(), _y, T(), _interpolant);
    return _x == output;
}

TEST_CASE("Step Sampler", "[runtime.animation.Sampler]")
{
    float interpolant = GENERATE(take(10, random(0.0f, 1.0f)));

    SECTION("Float")
    {
        float x = GENERATE(take(10, random(-1000.0f, 1000.0f)));
        float y = GENERATE(take(10, random(-1000.0f, 1000.0f)));
        REQUIRE(test_step(x, y, interpolant));
    }

    SECTION("Vec2")
    {
        ux3d::math::vec2 x(GENERATE(take(5, random(-1000.0f, 1000.0f))), GENERATE(take(5, random(-1000.0f, 1000.0f))));
        ux3d::math::vec2 y(GENERATE(take(5, random(-1000.0f, 1000.0f))), GENERATE(take(5, random(-1000.0f, 1000.0f))));
        REQUIRE(test_step(x, y, interpolant));
    }

    SECTION("Vec3")
    {
        ux3d::math::vec3 x(GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))));
        ux3d::math::vec3 y(GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))));
        REQUIRE(test_step(x, y, interpolant));
    }
}

template <class T>
bool test_linear(const T& _x, const T& _y, const float _interpolant)
{
    T output = ux3d::animation::linear(_x, T(), _y, T(), _interpolant);
    T result = (1 - _interpolant) * _x + _interpolant * _y;
    return result == output;
}

TEST_CASE("Linear Sampler", "[runtime.animation.Sampler]")
{
    float interpolant = GENERATE(take(10, random(0.0f, 1.0f)));

    SECTION("Float")
    {
        float x = GENERATE(take(10, random(-1000.0f, 1000.0f)));
        float y = GENERATE(take(10, random(-1000.0f, 1000.0f)));
        REQUIRE(test_linear(x, y, interpolant));
    }

    SECTION("Vec2")
    {
        ux3d::math::vec2 x(GENERATE(take(5, random(-1000.0f, 1000.0f))), GENERATE(take(5, random(-1000.0f, 1000.0f))));
        ux3d::math::vec2 y(GENERATE(take(5, random(-1000.0f, 1000.0f))), GENERATE(take(5, random(-1000.0f, 1000.0f))));
        REQUIRE(test_linear(x, y, interpolant));
    }

    SECTION("Vec3")
    {
        ux3d::math::vec3 x(GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))));
        ux3d::math::vec3 y(GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))));
        REQUIRE(test_linear(x, y, interpolant));
    }
}

template <class T>
bool test_cubic_spline(const std::vector<T>& _x, const std::vector<T>& _xTangent, const std::vector<T>& _y, const std::vector<T>& _yTangent, const float _interpolant)
{
    T output = ux3d::animation::cubic_spline(_x, _xTangent, _y, _yTangent, _interpolant);

    // check against a reference implementation;
    T reference = (2.0f * _interpolant * _interpolant * _interpolant - 3.0f * _interpolant * _interpolant + 1.0f) * _x
        + (_interpolant * _interpolant * _interpolant - 2.0f * _interpolant * _interpolant + _interpolant) * _xTangent
        + (-2.0f * _interpolant * _interpolant * _interpolant + 3.0f * _interpolant * _interpolant) * _y
        + (_interpolant * _interpolant * _interpolant - _interpolant * _interpolant) * _yTangent;

    return T(ux3d::math::epsilonEqual(output, reference, T(0.01f))) == T(true);
}

TEST_CASE("Cubic Spline Sampler", "[runtime.animation.Sampler]")
{
    /* float interpolant = GENERATE(take(1, random(0.0f, 1.0f)));

    SECTION("Float")
    {
        ux3d::animation::<float> x;
        ux3d::animation::SplineValue<float> y;
        x.value = GENERATE(take(5, random(-1000.0f, 1000.0f)));
        x.tangent = GENERATE(take(5, random(-1000.0f, 1000.0f)));
        y.value = GENERATE(take(5, random(-1000.0f, 1000.0f)));
        y.tangent = GENERATE(take(5, random(-1000.0f, 1000.0f)));
        REQUIRE(test_cubic_spline(x, y, interpolant));
    }

    SECTION("Vec2")
    {
        ux3d::animation::SplineValue<ux3d::math::vec2> x;
        ux3d::animation::SplineValue<ux3d::math::vec2> y;
        x.value = ux3d::math::vec2(GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))));
        x.tangent = ux3d::math::vec2(GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))));
        y.value = ux3d::math::vec2(GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))));
        y.tangent = ux3d::math::vec2(GENERATE(take(2, random(-1000.0f, 1000.0f))), GENERATE(take(2, random(-1000.0f, 1000.0f))));
        REQUIRE(test_cubic_spline(x, y, interpolant));
    }

    SECTION("Vec3")
    {
        ux3d::animation::SplineValue<ux3d::math::vec3> x;
        ux3d::animation::SplineValue<ux3d::math::vec3> y;
        x.value = ux3d::math::vec3(GENERATE(take(1, random(-1000.0f, 1000.0f))), GENERATE(take(1, random(-1000.0f, 1000.0f))), GENERATE(take(1, random(-1000.0f, 1000.0f))));
        x.tangent = ux3d::math::vec3(GENERATE(take(1, random(-1000.0f, 1000.0f))), GENERATE(take(1, random(-1000.0f, 1000.0f))), GENERATE(take(1, random(-1000.0f, 1000.0f))));
        y.value = ux3d::math::vec3(GENERATE(take(1, random(-1000.0f, 1000.0f))), GENERATE(take(1, random(-1000.0f, 1000.0f))), GENERATE(take(1, random(-1000.0f, 1000.0f))));
        y.tangent = ux3d::math::vec3(GENERATE(take(1, random(-1000.0f, 1000.0f))), GENERATE(take(1, random(-1000.0f, 1000.0f))), GENERATE(take(1, random(-1000.0f, 1000.0f))));
        REQUIRE(test_cubic_spline(x, y, interpolant));
    }

    // TODO: check more properties of cubic splines
    */
}
