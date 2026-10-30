// Copyright © 2020 UX3D GmbH. All rights reserved.

#include "renderer/Defines.h"
#include "core/Hash.h"

using namespace ux3d;
using namespace ux3d::renderer;

uint64_t Defines::entryHash(std::string_view name, int value)
{
    // FNV-1a over the name, mixed with the value through splitmix64 so that the
    // XOR of entry hashes is a well-distributed set hash.
    uint64_t h = 14695981039346656037ULL;
    for (const char c : name) {
        h ^= static_cast<uint8_t>(c);
        h *= 1099511628211ULL;
    }
    uint64_t z = h ^ (static_cast<uint64_t>(static_cast<uint32_t>(value)) * 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void Defines::define(std::string_view name, int value)
{
    auto it = m_defines.find(name);
    if (it == m_defines.end()) {
        m_defines.emplace(std::string(name), value);
        m_contentHash ^= entryHash(name, value);
        return;
    }
    if (it->second == value) {
        return;
    }
    m_contentHash ^= entryHash(name, it->second) ^ entryHash(name, value);
    it->second = value;
}

void Defines::defineIf(bool condition, std::string_view name, int value)
{
    if (condition) {
        define(name, value);
    } else {
        undefine(name);
    }
}

void Defines::undefine(std::string_view name)
{
    auto it = m_defines.find(name);
    if (it == m_defines.end()) {
        return;
    }
    m_contentHash ^= entryHash(name, it->second);
    m_defines.erase(it);
}

void Defines::clear()
{
    m_defines.clear();
    m_contentHash = 0;
}

void Defines::reserve(size_t capacity)
{
    m_defines.reserve(capacity);
}

std::string Defines::toLines() const
{
    std::string defineLines;
    defineLines.reserve(64 * m_defines.size());
    for (const auto& [name, value] : m_defines) {
        defineLines += "#define " + name + " " + std::to_string(value) + "\n";
    }
    return defineLines;
}

uint64_t Defines::hash() const
{
    core::Hasher hasher;
    for (const auto& [name, value] : m_defines) {
        hasher << name << value;
    }
    return hasher;
}

void Defines::defineAllOf(const Defines& rhs)
{
    for (const auto& [name, value] : rhs.m_defines) {
        define(name, value);
    }
}

bool Defines::isDefined(const std::string_view name) const
{
    return m_defines.find(name) != m_defines.end();
}
