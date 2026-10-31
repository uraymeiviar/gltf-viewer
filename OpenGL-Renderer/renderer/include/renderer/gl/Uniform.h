// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <atomic>
#include <cstdint>
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

    // Changes with every write that changes the value (bytes or element count) and is unique across
    // all uniforms, so two equal revisions always carry the same bytes (a copy keeps both). A write of
    // the bytes already held keeps the revision, so the program-side skip in commit() also covers
    // values re-written every frame (C8). 0 means never written.
    uint64_t revision() const { return m_revision; }

    // Cumulative, process-wide (C8): writes that kept the revision because the bytes were equal,
    // writes that took a new one, and commits skipped because the location already held the revision.
    struct Stats {
        std::atomic<uint64_t> equalWrites { 0 };
        std::atomic<uint64_t> changedWrites { 0 };
        std::atomic<uint64_t> commitSkips { 0 };
        std::atomic<uint64_t> commits { 0 };
    };
    static Stats& stats();

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
    const size_t size = count * sizeof(T);
    // C8: equal bytes and count keep the revision ("equal revision implies equal bytes" still holds).
    // A first write (revision 0) always takes a revision, even for an empty value.
    if (m_revision != 0 && m_count == count && m_bytes.size() == size
        && (size == 0 || memcmp(m_bytes.data(), values, size) == 0)) {
        stats().equalWrites.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    m_count = count;
    m_bytes.resize(size);
    if (size != 0) memcpy(m_bytes.data(), values, size);
    m_revision = nextRevision();
    stats().changedWrites.fetch_add(1, std::memory_order_relaxed);
}
}
