// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <type_traits>

namespace ux3d {
namespace core {
    template <typename Enum, typename Base = typename std::underlying_type_t<Enum>>
    class Flag {
    public:
        Flag() = default;
        constexpr Flag(const Enum _enum)
            : m_enum(_enum)
        {
        }

        constexpr Flag(const Flag& _other)
            : m_base(_other.m_base)
        {
        }
        constexpr Flag(Flag&& _other) noexcept
            : m_base(_other.m_base)
        {
        }

        Flag& operator=(const Flag& _other)
        {
            m_base = _other.m_base;
            return *this;
        }
        Flag& operator=(Flag&& _other)
        {
            m_base = _other.m_base;
            return *this;
        }

        Flag& operator=(const Enum& _enum)
        {
            m_enum = _enum;
            return *this;
        }
        Flag& operator&=(const Enum& _enum)
        {
            m_base &= static_cast<Base>(_enum);
            return *this;
        }
        Flag& operator|=(const Enum& _enum)
        {
            m_base |= static_cast<Base>(_enum);
            return *this;
        }
        Flag& operator^=(const Enum& _enum)
        {
            m_base ^= static_cast<Base>(_enum);
            return *this;
        }

        constexpr bool isSet(const Enum& _enum) const { return (m_base & static_cast<Base>(_enum)) == static_cast<Base>(_enum); }

        constexpr operator Enum() const { return m_enum; }
        constexpr operator Base() const { return m_base; }

    private:
        union {
            Base m_base = 0;
            Enum m_enum;
        };
    };

    template <typename Enum>
    constexpr Flag<Enum> operator|(Flag<Enum> f0, Flag<Enum> f1)
    {
        return Flag(std::underlying_type_t<Enum>(f0) | std::underlying_type_t<Enum>(f1));
    }

    template <typename Enum>
    constexpr Flag<Enum> operator&(Flag<Enum> f0, Flag<Enum> f1)
    {
        return Flag(std::underlying_type_t<Enum>(f0) & std::underlying_type_t<Enum>(f1));
    }

    template <typename Enum>
    constexpr Flag<Enum> operator^(Flag<Enum> f0, Flag<Enum> f1)
    {
        return Flag(std::underlying_type_t<Enum>(f0) ^ std::underlying_type_t<Enum>(f1));
    }
}
} // !ux3d
