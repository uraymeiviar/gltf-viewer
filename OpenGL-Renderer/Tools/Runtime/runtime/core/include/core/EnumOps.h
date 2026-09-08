// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <type_traits>
namespace ux3d {
template <typename EnumType, typename = std::enable_if_t<std::is_enum<EnumType>::value>>
constexpr EnumType operator|(EnumType f0, EnumType f1)
{
    return EnumType(std::underlying_type_t<EnumType>(f0) | std::underlying_type_t<EnumType>(f1));
}

template <typename EnumType, typename = std::enable_if_t<std::is_enum<EnumType>::value>>
constexpr EnumType operator&(EnumType f0, EnumType f1)
{
    return EnumType(std::underlying_type_t<EnumType>(f0) & std::underlying_type_t<EnumType>(f1));
}

template <typename EnumType, typename = std::enable_if_t<std::is_enum<EnumType>::value>>
constexpr EnumType operator^(EnumType f0, EnumType f1)
{
    return EnumType(std::underlying_type_t<EnumType>(f0) ^ std::underlying_type_t<EnumType>(f1));
}

namespace core {
    template <typename EnumType, typename = std::enable_if_t<std::is_enum<EnumType>::value>>
    constexpr bool CheckFlag(EnumType f0, EnumType f1)
    {
        return (std::underlying_type_t<EnumType>(f0) & std::underlying_type_t<EnumType>(f1)) != 0;
    }
}

template <typename EnumType, typename T, typename = std::enable_if_t<std::is_enum<EnumType>::value && std::is_integral<T>::value>>
constexpr bool operator<(T f0, EnumType f1)
{
    return f0 < std::underlying_type_t<EnumType>(f1);
}

template <typename EnumType, typename T, typename = std::enable_if_t<std::is_enum<EnumType>::value && std::is_integral<T>::value>>
constexpr bool operator<=(T f0, EnumType f1)
{
    return f0 <= std::underlying_type_t<EnumType>(f1);
}

template <typename EnumType, typename T, typename = std::enable_if_t<std::is_enum<EnumType>::value && std::is_integral<T>::value>>
constexpr bool operator>(EnumType f0, T f1)
{
    return std::underlying_type_t<EnumType>(f0) > f1;
}

template <typename EnumType, typename T, typename = std::enable_if_t<std::is_enum<EnumType>::value && std::is_integral<T>::value>>
constexpr bool operator>=(EnumType f0, T f1)
{
    return std::underlying_type_t<EnumType>(f0) >= f1;
}
} // !ux3d
