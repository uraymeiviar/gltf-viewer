// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <type_traits>
#include <variant>

namespace ux3d::core {
template <typename... VariantTypes>
struct VariantWrapper {
    std::variant<VariantTypes...> variant;

    template <typename Type>
    bool hasType() const { return std::get_if<Type>(&variant) != nullptr; }

    template <typename Type>
    Type& unsafeGet() { return *std::get_if<Type>(&variant); }

    template <typename Type>
    const Type& unsafeGet() const { return *std::get_if<Type>(&variant); }

    template <typename Type>
    Type get()
    {
        static_assert(std::is_pointer<Type>::value, "'safe' get is only available for pointers");

        if (hasType<Type>()) {
            return unsafeGet<Type>();
        }
        return static_cast<Type>(nullptr);
    }

    bool operator==(const VariantWrapper& _other) const { return _other.variant == variant; }
    bool operator!=(const VariantWrapper& _other) const { return _other.variant != variant; }

    VariantWrapper& operator=(const VariantWrapper& _other);
    VariantWrapper& operator=(VariantWrapper&& _other);

    VariantWrapper() = default;
    ~VariantWrapper() = default;
    VariantWrapper(const VariantWrapper& _other)
        : variant(_other.variant)
    {
    }
    VariantWrapper(VariantWrapper&& _other)
        : variant(std::move(_other.variant))
    {
    }

    template <typename... ParamTypes,
        typename = std::enable_if_t<std::is_constructible_v<std::variant<VariantTypes...>, ParamTypes...>>>
    VariantWrapper(ParamTypes&&... _parameters)
        : variant(std::forward<ParamTypes>(_parameters)...)
    {
    }
};

template <typename... VariantTypes>
VariantWrapper<VariantTypes...>& VariantWrapper<VariantTypes...>::operator=(const VariantWrapper<VariantTypes...>& _other)
{
    if (this != &_other) {
        variant = _other.variant;
    }
    return *this;
}

template <typename... VariantTypes>
VariantWrapper<VariantTypes...>& VariantWrapper<VariantTypes...>::operator=(VariantWrapper<VariantTypes...>&& _other)
{
    if (this != &_other) {
        variant = std::move(_other.variant);
    }
    return *this;
}
}
