// Copyright © 2020 UX3D GmbH. All rights reserved.

#include "renderer/Defines.h"
#include "core/Hash.h"

using namespace ux3d;
using namespace ux3d::renderer;

void Defines::define(std::string_view name, int value)
{
    m_defines[std::string(name)] = value;
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
    m_defines.erase(std::string(name));
}

void Defines::clear()
{
    m_defines.clear();
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
        m_defines[name] = value;
    }
}

bool Defines::isDefined(const std::string_view name) const
{
    return m_defines.find(std::string(name)) != m_defines.end();
}
