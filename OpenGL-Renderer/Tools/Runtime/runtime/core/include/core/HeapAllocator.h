// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Allocator.h"
#include <new> // placement new
#include <utility>

namespace ux3d {
namespace core {
    class HeapAllocator : public IAllocator {
    public:
        // currently all allocations are aligned by DefaultAlignment bytes
        void* allocate(size_t n, uint32_t alignment = DefaultAlignment) final;

        template <class T>
        T* allocate(size_t alignment = alignof(T));

        template <class T, class... Args>
        T* allocateAndConstruct(Args&&... _args);

        void deallocate(void* p, size_t n = 0) final;

        static HeapAllocator* defaultInstance()
        {
            static HeapAllocator defaultInst;
            return &defaultInst;
        }
    };

    template <class T>
    T* HeapAllocator::allocate(size_t alignment)
    {
        return reinterpret_cast<T*>(allocate(sizeof(T), alignment));
    }

    template <class T, class... Args>
    inline T* HeapAllocator::allocateAndConstruct(Args&&... _args)
    {
        T* ptr = reinterpret_cast<T*>(allocate(sizeof(T), alignof(T)));
        if (ptr != nullptr) {
            new (ptr) T(std::forward<Args>(_args)...);
        }
        return ptr;
    }
}
} // !ux3d
