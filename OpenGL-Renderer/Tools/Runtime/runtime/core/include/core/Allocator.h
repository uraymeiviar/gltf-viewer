// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "MemoryUtilities.h" // DefaultAlignment
#include <type_traits>

namespace ux3d {
namespace core {
    class IAllocator {
    public:
        virtual ~IAllocator() {};
        virtual void* allocate(size_t bytes, uint32_t alignment = DefaultAlignment) = 0;

        // some allocators (LinearAllocator) dont support deallocation
        virtual void deallocate(void* ptr, size_t bytes)
        {
            (void)ptr;
            (void)bytes;
        };
    };

    // Allocator interface:
    // void* allocate(size_t bytes, size_t alignment);
    // void  deallocate(void* p, size_t bytes);

    template <typename T>
    struct canAllocate {
        template <typename U, void* (U::*)(size_t, uint32_t)>
        struct Check;
        template <typename U>
        static std::true_type test(Check<U, &U::allocate>*);
        template <typename U>
        static std::false_type test(...);
        static constexpr bool value = std::is_same<decltype(test<T>(0)), std::true_type>::value;
    };

    template <typename T, typename std::enable_if<canAllocate<T>::value>::type* = 0>
    decltype(auto) allocateIfExists(T* t, size_t n, uint32_t alignment) { return t->allocate(n, alignment); }
    template <typename T, typename std::enable_if<!canAllocate<T>::value>::type* = 0>
    decltype(auto) allocateIfExists(T* t, size_t n, uint32_t alignment)
    {
        (void)t, (void)n, (void)alignment;
        return nullptr;
    }

    template <typename T>
    struct canDeallocate {
        template <typename U, void (U::*)(void*, size_t)>
        struct Check;
        template <typename U>
        static std::true_type test(Check<U, &U::deallocate>*);
        template <typename U>
        static std::false_type test(...);
        static constexpr bool value = std::is_same<decltype(test<T>(0)), std::true_type>::value;
    };

    template <typename T>
    typename std::enable_if<canDeallocate<T>::value>::type deallocateIfExists(T* t, void* p, size_t bytes = 0u) { t->deallocate(p, bytes); }
    template <typename T>
    typename std::enable_if<!canDeallocate<T>::value>::type deallocateIfExists(T* t, void* p, size_t bytes = 0u)
    {
        (void)t;
        (void)p;
        (void)bytes;
    }
} // !core
} // !ux3d
