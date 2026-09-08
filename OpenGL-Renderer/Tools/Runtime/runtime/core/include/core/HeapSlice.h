// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "HeapAllocator.h"
#include "Slice.h"

namespace ux3d {
namespace core {
    template <class T>
    class HeapSlice : public Slice<T> {
    public:
        using Slice<T>::Slice;

        HeapSlice()
            : Slice<T>(HeapAllocator::defaultInstance())
        {
        }

        bool allocate(size_t _Elements, uint32_t _Alignment) { return Slice<T>::allocate(_Elements, _Alignment, HeapAllocator::defaultInstance()); }
        bool allocate(size_t _Elements) { return Slice<T>::allocate(_Elements, DefaultAlignment, HeapAllocator::defaultInstance()); }

        bool resize(size_t _Elements, uint32_t _Alignment) { return Slice<T>::resize(_Elements, _Alignment, HeapAllocator::defaultInstance()); }
        bool resize(size_t _Elements) { return Slice<T>::resize(_Elements, DefaultAlignment, HeapAllocator::defaultInstance()); }

        bool resizeWithValue(size_t _Elements, uint32_t _Alignment, const T& _defaultValue) { return Slice<T>::resize(_Elements, _Alignment, _defaultValue, HeapAllocator::defaultInstance()); }
        bool resizeWithValue(size_t _Elements, const T& _defaultValue) { return Slice<T>::resize(_Elements, DefaultAlignment, _defaultValue, HeapAllocator::defaultInstance()); }
    };

    template <class T, class U>
    HeapSlice<T> asSlice(HeapSlice<U>&& _Other) { return HeapSlice<T>(std::forward<HeapSlice<U>>(_Other)); }

} // !core
} // !ux3d
