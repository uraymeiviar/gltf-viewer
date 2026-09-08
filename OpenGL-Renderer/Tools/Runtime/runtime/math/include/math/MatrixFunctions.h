// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/transform.hpp>
#include <matrix.hpp>

namespace ux3d::math {
using namespace glm;

template <typename T>
inline bool getTranslation(const mat<4, 4, T>& _Transform, vec<3, T>& _OutTranslation)
{
    using Vec3 = vec<3, T>;
    using Vec4 = vec<4, T>;
    using Quat = qua<T>;

    Vec3 scale;
    Vec3 skew;
    Quat rotation;
    Vec4 perspective;
    return decompose(_Transform, scale, rotation, _OutTranslation, skew, perspective);
}

template <typename T>
inline bool getRotation(const mat<4, 4, T>& _Transform, qua<T>& _OutRotation)
{
    using Vec3 = vec<3, T>;
    using Vec4 = vec<4, T>;

    Vec3 scale;
    Vec3 translation;
    Vec3 skew;
    Vec4 perspective;
    return decompose(_Transform, scale, _OutRotation, translation, skew, perspective);
}

template <typename T>
inline bool getScale(const mat<4, 4, T>& _Transform, vec<3, T>& _OutScale)
{
    using Vec3 = vec<3, T>;
    using Vec4 = vec<4, T>;
    using Quat = qua<T>;

    Vec3 translation;
    Vec3 skew;
    Quat rotation;
    Vec4 perspective;
    return decompose(_Transform, _OutScale, rotation, translation, skew, perspective);
}
}
