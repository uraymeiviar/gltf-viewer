// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "MemoryIterator.h"

namespace ux3d::core {
template <class T>
class MemoryRange {
public:
    constexpr MemoryRange()
        : m_begin(nullptr)
        , m_end(nullptr)
    {
    }

    constexpr MemoryRange(T* _pStart, size_t _elements)
        : m_begin(_pStart)
        , m_end(_pStart + _elements)
    {
    }

    constexpr MemoryRange(T* _pStart, T* _pEnd)
        : m_begin(_pStart)
        , m_end(_pEnd)
    {
    }

    constexpr MemoryRange(const MemoryIterator<T>& _begin, const MemoryIterator<T>& _end)
        : m_begin(_begin)
        , m_end(_end)
    {
    }

    template <size_t size>
    constexpr explicit MemoryRange(T (&_array)[size])
        : m_begin(&_array[0])
        , m_end(&_array[0] + size)
    {
    }

    constexpr MemoryRange(const MemoryRange& _other)
        : m_begin(_other.begin())
        , m_end(_other.end())
    {
    }

    MemoryRange& operator=(const MemoryRange& _other)
    {
        m_begin = _other.begin();
        m_end = _other.end();
        return *this;
    }

    operator MemoryRange<const T>() const { return MemoryRange<const T>(m_begin, m_end); }

    const MemoryIterator<T>& begin() const { return m_begin; }
    const MemoryIterator<T>& end() const { return m_end; }

    decltype(auto) cbegin() const { return m_begin.getConst(); }
    decltype(auto) cend() const { return m_end.getConst(); }

    T& operator[](size_t _index) { return *(m_begin + static_cast<typename decltype(m_begin)::difference_type>(_index)); }

    size_t size() const
    {
        return static_cast<size_t>(m_end - m_begin);
    }

    bool empty() const
    {
        return size() == 0u;
    }

    T* data() const { return m_begin.operator->(); }

    decltype(auto) front() const { return *m_begin; }
    decltype(auto) back() const { return *(m_end - 1); }

    size_t byteSize() const { return reinterpret_cast<uintptr_t>((m_end - 1).operator->() + 1) - reinterpret_cast<uintptr_t>(m_begin.operator->()); } // accessing end iterator directly is undefined behavior

    // convert from one type to another view of the same data
    template <typename U>
    MemoryRange<U> viewAs() const { return MemoryRange<U>(reinterpret_cast<U*>(m_begin.operator->()), reinterpret_cast<U*>((m_end - 1).operator->() + 1)); } // accessing end iterator directly is undefined behavior

private:
    MemoryIterator<T> m_begin;
    MemoryIterator<T> m_end;
};

template <class T>
constexpr MemoryRange<T> makeRange(T* _pStart, size_t _elements) { return MemoryRange<T>(_pStart, _elements); }

template <class T>
constexpr MemoryRange<T> makeRange(T* _pStart, T* _pEnd) { return MemoryRange<T>(_pStart, _pEnd); }

template <class T, class U>
constexpr MemoryRange<T> makeRange(const MemoryRange<U>& _other) { return MemoryRange<T>(reinterpret_cast<T*>(_other.begin().operator->()), reinterpret_cast<T*>((_other.end() - 1).operator->() + 1)); } // accessing end iterator directly is undefined behavior

template <class T, size_t size>
constexpr MemoryRange<T> makeRange(T (&_array)[size]) { return MemoryRange<T>(_array); }

template <class C>
constexpr MemoryRange<uint8_t> makeMutableByteRange(C& contiguousContainer)
{
    return MemoryRange(
        reinterpret_cast<uint8_t*>(contiguousContainer.begin().operator->()),
        reinterpret_cast<uint8_t*>((contiguousContainer.end() - 1).operator->() + 1)); // accessing end iterator directly is undefined behavior
}

template <class C>
constexpr MemoryRange<const uint8_t> makeByteRange(const C& contiguousContainer)
{
    return MemoryRange(
        reinterpret_cast<const uint8_t*>(contiguousContainer.begin().operator->()),
        reinterpret_cast<const uint8_t*>((contiguousContainer.end() - 1).operator->() + 1)); // accessing end iterator directly is undefined behavior
}

}
