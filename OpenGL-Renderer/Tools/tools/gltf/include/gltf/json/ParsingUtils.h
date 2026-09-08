// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "core/String.h"
#include "math/MatrixTypes.h"
#include "math/VectorTypes.h"

namespace ux3d::gltf {
constexpr int32_t UndefinedIndex = -1;

enum class Index : uint32_t {
    Valid,
    Undefined,
    OutOfRange
};

template <math::length_t VectorLength, class Container>
bool isValidVector(const Container& _Doubles)
{
    return _Doubles.size() == VectorLength;
}

template <class Container>
bool isValidQuat(const Container& _Doubles)
{
    return isValidVector<4>(_Doubles);
}

template <class Container>
bool isValid4x4(const Container& _Doubles)
{
    return _Doubles.size() == 4 * 4;
}

template <math::length_t VectorLength, class JsonArray>
math::vec<VectorLength, double> jsonArrayToVector(const JsonArray& _JsonArray)
{
    math::vec<VectorLength, double> result;
    for (math::length_t i = 0; i < VectorLength; ++i) {
        result[i] = _JsonArray[i].template Get<double>();
    }
    return result;
}

template <math::length_t VectorLength, class Container>
math::vec<VectorLength, double> doublesToVector(const Container& _Doubles)
{
    math::vec<VectorLength, double> result;
    for (math::length_t i = 0; i < VectorLength; ++i) {
        result[i] = _Doubles[static_cast<size_t>(i)];
    }
    return result;
}

template <class Container>
math::dquat doublesToQuat(const Container& _Doubles)
{
    math::dquat quat;
    quat.x = _Doubles[0];
    quat.y = _Doubles[1];
    quat.z = _Doubles[2];
    quat.w = _Doubles[3];
    return quat;
}

template <math::length_t Width, math::length_t Height, class Container>
math::mat<Width, Height, double> doublesToMatrix(const Container& _Doubles)
{
    math::mat<Width, Height, double> result;
    for (math::length_t x = 0; x < Width; ++x) {
        for (math::length_t y = 0; y < Height; ++y) {
            result[x][y] = _Doubles[static_cast<size_t>(x * 4 + y)];
        }
    }
    return result;
}

template <math::length_t VectorLength, class Container>
void vectorToDoubles(const math::vec<VectorLength, double>& _Vector, Container& _Doubles)
{
    _Doubles.clear();
    for (math::length_t i = 0; i < VectorLength; ++i) {
        _Doubles.emplace_back(double(_Vector[i]));
    }
}

template <math::length_t Width, math::length_t Height, class Container>
void matToDoubles(const math::mat<Width, Height, double>& _Matrix, Container& _Doubles)
{
    _Doubles.clear();
    for (math::length_t x = 0; x < Width; ++x) {
        for (math::length_t y = 0; y < Height; ++y) {
            _Doubles.emplace_back(double(_Matrix[x][y]));
        }
    }
}

template <class Container>
void quatToDoubles(const math::dquat _Quat, Container& _Doubles)
{
    _Doubles.clear();

    _Doubles.emplace_back(double(_Quat.x));
    _Doubles.emplace_back(double(_Quat.y));
    _Doubles.emplace_back(double(_Quat.z));
    _Doubles.emplace_back(double(_Quat.w));
}

template <class Container>
void removeDuplicates(Container& _Container)
{
    auto first = _Container.begin();
    auto last = _Container.end();

    while (first != last) {
        auto next(first);
        last = std::remove(++next, last, *first);
        first = next;
    }

    _Container.erase(last, _Container.end());
}

template <class JsonDictionary>
bool hasProperty(const JsonDictionary& _Dictionary, const core::string& _Property)
{
    return _Dictionary.find(_Property) != _Dictionary.end();
}

template <class JsonDictionary>
int32_t getIndex(const JsonDictionary& _Dictionary, const core::string& _Property)
{
    return _Dictionary.at(_Property).TextureIndex();
}

template <class JsonDictionary>
float getFloat(const JsonDictionary& _Dictionary, const core::string& _Property)
{
    return static_cast<float>(_Dictionary.at(_Property).number_value);
}

template <class JsonDictionary>
math::dvec3 getVec3(const JsonDictionary& _Dictionary, const core::string& _Property)
{
    const auto& doubles = _Dictionary.at(_Property).number_array;
    return doublesToVector<math::dvec3>(doubles);
}

template <class JsonDictionary>
math::dvec4 getVec4(const JsonDictionary& _Dictionary, const core::string& _Property)
{
    const auto& doubles = _Dictionary.at(_Property).number_array;
    return doublesToVector<math::dvec4>(doubles);
}

template <class JsonDictionary>
bool getBool(const JsonDictionary& _Dictionary, const core::string& _Property)
{
    return _Dictionary.at(_Property).bool_value;
}

template <class JsonDictionary>
core::string getString(const JsonDictionary& _Dictionary, const core::string& _Property)
{
    return _Dictionary.at(_Property).string_value;
}

template <class T, class JsonObject>
T typedJsonMember(const JsonObject& _Object, const core::string& _Name)
{
    return _Object.Get(_Name).template Get<T>();
}

template <class List>
Index indexValidity(const List& _List, int32_t _Index)
{
    if (_Index == UndefinedIndex) {
        return Index::Undefined;
    }
    if (_Index >= 0 && _Index < static_cast<int32_t>(_List.size())) {
        return Index::Valid;
    }
    return Index::OutOfRange;
}
}
