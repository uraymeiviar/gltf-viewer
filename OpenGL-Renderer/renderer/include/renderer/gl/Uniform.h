// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace ux3d::renderer::gl {
class GL;
struct UniformBinding;

class Uniform {
public:
    Uniform(const std::string& name);

    template <class T>
    Uniform(const std::string& name, const T& value);

    template <class T>
    void write(const T& value);

    void write(bool value);

    template <class T>
    void write(const T* values, size_t count);

    void commit(GL* gl, const UniformBinding& binding) const;

    // The value as written, e.g. to copy it into a uniform block.
    const uint8_t* bytes() const { return m_bytes.data(); }
    size_t byteSize() const { return m_bytes.size(); }

    // Changes with every write and is unique across all uniforms, so two equal revisions
    // always carry the same bytes (a copy keeps both). 0 means never written.
    uint64_t revision() const { return m_revision; }

    std::string name;

private:
    static uint64_t nextRevision();

    size_t m_count = 1;
    std::vector<uint8_t> m_bytes;
    uint64_t m_revision = 0;
};

template <class T>
Uniform::Uniform(const std::string& name, const T& value)
    : Uniform(name)
{
    write(value);
}

template <class T>
void Uniform::write(const T& value)
{
    write(&value, 1);
}

template <class T>
void Uniform::write(const T* values, size_t count)
{
    m_count = count;
    m_bytes.resize(count * sizeof(T));
    memcpy(m_bytes.data(), values, count * sizeof(T));
    m_revision = nextRevision();
}
}
