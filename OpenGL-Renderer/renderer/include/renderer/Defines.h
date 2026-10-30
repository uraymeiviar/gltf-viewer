// Copyright © 2020 UX3D GmbH. All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
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

    /// Order-independent hash of the current definitions, kept up to date by
    /// every change (P4.6). Equal definition sets have equal content hashes
    /// regardless of insertion order; copies keep the hash.
    uint64_t contentHash() const { return m_contentHash; }

private:
    static uint64_t entryHash(std::string_view name, int value);

    // C5 (round 11): heterogeneous lookup, so define/undefine/isDefined of an existing name never builds a
    // std::string. The hash is std::hash<std::string_view>, which the standard requires to equal
    // std::hash<std::string> for the same characters, so buckets and iteration order (toLines, hash) are unchanged.
    struct NameHash {
        using is_transparent = void;
        size_t operator()(std::string_view name) const noexcept { return std::hash<std::string_view>{}(name); }
    };

    std::unordered_map<std::string, int, NameHash, std::equal_to<>> m_defines;
    uint64_t m_contentHash = 0;
};
}
