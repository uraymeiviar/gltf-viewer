// Copyright © 2020 UX3D GmbH. All rights reserved.

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace ux3d::renderer {
class Defines {
public:
    /// Defines a constant as some value, which defaults to 1.
    void define(std::string_view name, int value = 1);

    /// Removes the definition of the constant. This is not equivalent to defining the constant as 0.
    void undefine(std::string_view name);

    /// Defines the constant if the condition holds, otherwise removes the definition.
    void defineIf(bool condition, std::string_view name, int value = 1);

    /// Removes all definitions.
    void clear();

    /// Reserves space for at least [capacity] definitions.
    void reserve(size_t capacity);

    /// Print all definitions according to the C language: #define [name] [value].
    std::string toLines() const;

    /// Computes a hash based on the constant names and their values.
    uint64_t hash() const;

    /// Adds definitions from [rhs].
    /// In case of a name-confict, the value from [rhs] is taken.
    void defineAllOf(const Defines& rhs);

    /// Returns true if the name defined
    bool isDefined(std::string_view name) const;

private:
    std::unordered_map<std::string, int> m_defines;
};
}
