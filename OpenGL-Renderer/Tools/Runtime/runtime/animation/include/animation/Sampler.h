// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴

#pragma once

#include "math/VectorFunctions.h"
#include "math/VectorTypes.h"

#include <vector>

namespace ux3d::animation {

template <class T, class Func>
std::vector<T> apply(const Func& func, const std::vector<T>& lower, const std::vector<T>& lowerTangent, const std::vector<T>& higher, const std::vector<T>& higherTangent, float interpolant)
{
    std::vector<T> output;

    output.resize(lower.size());
    if (lower.size() == lowerTangent.size() && lowerTangent.size() == higher.size() && higher.size() == higherTangent.size()) {
        for (size_t i = 0; i < output.size(); ++i) {
            output[i] = func(lower[i], lowerTangent[i], higher[i], higherTangent[i], interpolant);
        }
    } else if (lower.size() == higher.size()) {
        // No tangents present => linear or step interpolation
        for (size_t i = 0; i < output.size(); ++i) {
            output[i] = func(lower[i], {}, higher[i], {}, interpolant);
        }
    }

    return output;
}

template <class T>
T step(const T& lower, const T&, const T&, const T&, float)
{
    return lower;
}

template <class T>
T linear(const T& lower, const T&, const T& higher, const T&, float interpolant)
{
    return (1.0f - interpolant) * lower + interpolant * higher;
}

template <>
math::quat linear(const math::quat& lower, const math::quat&, const math::quat& higher, const math::quat&, float interpolant)
{
    return math::slerp(lower, higher, interpolant);
}

template <>
std::vector<float> linear(const std::vector<float>& lower, const std::vector<float>&, const std::vector<float>& higher, const std::vector<float>&, float interpolant)
{
    return apply(linear<float>, lower, {}, higher, {}, interpolant);
}

template <class T>
T cubic_spline(const T& lower, const T& lowerTangent, const T& higher, const T& higherTangent, float interpolant)
{
    // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#appendix-c-spline-interpolation
    //  p(t) = (2t3 - 3t2 + 1)p0 + (t3 - 2t2 + t)m0 + (-2t3 + 3t2)p1 + (t3 - t2)m1
    float t3 = interpolant * interpolant * interpolant;
    float t2 = interpolant * interpolant;
    float t = interpolant;

    return (2.0f * t3 - 3.0f * t2 + 1.0f) * lower
        + (t3 - 2.0f * t2 + t) * lowerTangent
        + (-2.0f * t3 + 3.0f * t2) * higher
        + (t3 - t2) * higherTangent;
}

template <>
math::quat cubic_spline(const math::quat& lower, const math::quat& lowerTangent, const math::quat& higher, const math::quat& higherTangent, float interpolant)
{
    // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#appendix-c-spline-interpolation
    //  p(t) = (2t3 - 3t2 + 1)p0 + (t3 - 2t2 + t)m0 + (-2t3 + 3t2)p1 + (t3 - t2)m1
    float t3 = interpolant * interpolant * interpolant;
    float t2 = interpolant * interpolant;
    float t = interpolant;

    return math::normalize((2.0f * t3 - 3.0f * t2 + 1.0f) * lower
        + (t3 - 2.0f * t2 + t) * lowerTangent
        + (-2.0f * t3 + 3.0f * t2) * higher
        + (t3 - t2) * higherTangent);
}

template <>
std::vector<math::quat> cubic_spline(const std::vector<math::quat>& lower, const std::vector<math::quat>& lowerTangent, const std::vector<math::quat>& higher, const std::vector<math::quat>& higherTangent, float interpolant)
{
    return apply(cubic_spline<math::quat>, lower, lowerTangent, higher, higherTangent, interpolant);
}

template <>
std::vector<math::vec3> cubic_spline(const std::vector<math::vec3>& lower, const std::vector<math::vec3>& lowerTangent, const std::vector<math::vec3>& higher, const std::vector<math::vec3>& higherTangent, float interpolant)
{
    return apply(cubic_spline<math::vec3>, lower, lowerTangent, higher, higherTangent, interpolant);
}

template <>
std::vector<float> cubic_spline(const std::vector<float>& lower, const std::vector<float>& lowerTangent, const std::vector<float>& higher, const std::vector<float>& higherTangent, float interpolant)
{
    return apply(cubic_spline<float>, lower, lowerTangent, higher, higherTangent, interpolant);
}
}
