// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "DerivedEnum.h"

namespace ux3d {
namespace core {
    enum class ResultE : uint32_t {
        Success = 0u
    };

    constexpr uint32_t DerivedResultStart = 1u;

    template <typename EnumType, typename std::enable_if<std::is_enum<EnumType>::value>::type* = 0>
    constexpr bool Failed(EnumType _Result)
    {
        return std::underlying_type_t<EnumType>(_Result) != std::underlying_type_t<ResultE>(ResultE::Success);
    }

    template <typename EnumType, typename std::enable_if<std::is_enum<EnumType>::value>::type* = 0>
    constexpr bool Succeeded(EnumType _Result)
    {
        return std::underlying_type_t<EnumType>(_Result) == std::underlying_type_t<ResultE>(ResultE::Success);
    }

    template <class Enum>
    struct ResultT : public DerivedEnum<Enum, ResultE> {
        using DerivedEnum<Enum, ResultE>::DerivedEnum;

        static constexpr Enum Success = Enum(ResultE::Success);

        operator bool() const { return DerivedEnum<Enum, ResultE>::base == ResultE::Success; }

        ResultT(bool) = delete;
    };

    using Result = ResultT<ResultE>;

    template <class Enum>
    inline bool Failed(ResultT<Enum> _Result) { return _Result.base != ResultE::Success; }

    template <class Enum>
    inline bool Succeeded(ResultT<Enum> _Result) { return _Result.base == ResultE::Success; }

    template <class Enum, typename Underlying = typename std::underlying_type<Enum>::type>
    inline ResultT<Enum> Max(ResultT<Enum> _A, ResultT<Enum> _B)
    {
        return static_cast<Underlying>(_A.base) > static_cast<Underlying>(_B.base) ? _A : _B;
    }

    template <class Enum, typename Underlying = typename std::underlying_type<Enum>::type>
    inline ResultT<Enum> Max(Enum _A, ResultT<Enum> _B)
    {
        return static_cast<Underlying>(_A) > static_cast<Underlying>(_B.base) ? ResultT<Enum>(_A) : _B;
    }

    template <class Enum, typename Underlying = typename std::underlying_type<Enum>::type>
    inline ResultT<Enum> Max(ResultT<Enum> _A, Enum _B)
    {
        return static_cast<Underlying>(_A.base) > static_cast<Underlying>(_B) ? _A : ResultT<Enum>(_B);
    }
} // !core
} // !ux3d
