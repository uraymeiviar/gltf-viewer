// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <cstdint>
#include <type_traits>

namespace ux3d {
namespace core {
    template <class Enum, class BaseEnum, typename = std::enable_if_t<std::is_enum<Enum>::value && std::is_enum<BaseEnum>::value>>
    struct DerivedEnum {
        DerivedEnum()
            : base()
        {
        }
        DerivedEnum(BaseEnum _Base)
            : base(_Base)
        {
        }

        template <
            typename B = BaseEnum,
            typename E = Enum,
            typename = std::enable_if_t<!std::is_same<B, E>::value>>
        DerivedEnum(Enum _Derived)
            : derived(_Derived)
        {
        }

        DerivedEnum(const DerivedEnum& _Other)
            : derived(_Other.derived)
        {
        }
        DerivedEnum(DerivedEnum&& _Other) noexcept
            : derived(_Other.derived)
        {
        }

        DerivedEnum(std::underlying_type_t<BaseEnum> _Base)
            : base(static_cast<BaseEnum>(_Base))
        {
        }

        // dont implement if underlying type is the same
        template <
            typename B = std::underlying_type_t<BaseEnum>,
            typename E = std::underlying_type_t<Enum>,
            typename = std::enable_if_t<!std::is_same<B, E>::value>>
        DerivedEnum(std::underlying_type_t<Enum> _Derived)
            : derived(_Derived)
        {
        }

        DerivedEnum& operator=(const DerivedEnum& _Other)
        {
            derived = _Other.derived;
            return *this;
        }
        DerivedEnum& operator=(DerivedEnum&& _Other) noexcept
        {
            derived = _Other.derived;
            return *this;
        }

        operator BaseEnum() const { return base; }

        template <
            typename B = BaseEnum,
            typename E = Enum,
            typename = std::enable_if_t<!std::is_same<B, E>::value>>
        operator Enum() const { return derived; }

        operator std::underlying_type_t<Enum>() const { return derived; }

        union {
            BaseEnum base;
            Enum derived;
        };
    };
} // !core
} // !ux3d
