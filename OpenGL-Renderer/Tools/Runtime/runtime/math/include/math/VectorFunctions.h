// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <common.hpp>
#include <exponential.hpp>
#include <geometric.hpp>
#include <gtc/color_space.hpp>
#include <gtc/constants.hpp>
#include <gtc/epsilon.hpp>
#include <gtx/vector_angle.hpp>
#include <integer.hpp>
#include <packing.hpp>
#include <trigonometric.hpp>
#include <vector_relational.hpp>

namespace ux3d::math {
using namespace glm;

constexpr auto NormalizationEpsilon = 1e-6;

template <glm::length_t Size, typename ValueType>
inline bool isNormalized(const vec<Size, ValueType>& _Vector, const ValueType _epsilon = NormalizationEpsilon)
{
    return epsilonEqual(length(_Vector), 1.0, _epsilon);
}

template <typename ValueType>
inline bool isNormalized(const qua<ValueType>& _Quat, const ValueType _epsilon = NormalizationEpsilon)
{
    return epsilonEqual(length(_Quat), 1.0, _epsilon);
}
}
