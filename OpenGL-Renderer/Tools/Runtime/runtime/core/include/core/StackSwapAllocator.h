// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "HeapAllocator.h"
#include "Logger.h"

namespace ux3d {
namespace core {
    /// Reserves a static amount of bytes on the stack.
    /// Allocations are maid on the stack linearly until the stack memory is exhausted.
    /// As soon as the stack memory failed to host an allocation request, the used stack memory is copied
    /// into the swap allocator to avoid splitting up the allocated user-data.
    /// Note however that this implies that there is no pointer stability on elements into the stack memory.
    template <size_t stack_capacity, class SwapAllocator = HeapAllocator>
    class StackSwapAllocator : public IAllocator {
    public:
        using swap_allocator_type = SwapAllocator;

        constexpr explicit StackSwapAllocator(SwapAllocator* _SwapAllocator)
            : m_pSwapAllocator(_SwapAllocator)
        {
        }

        constexpr StackSwapAllocator()
            : m_pSwapAllocator(nullptr)
        {
        }

        StackSwapAllocator(const StackSwapAllocator& _Other) = delete;
        StackSwapAllocator(StackSwapAllocator&& _Other) = delete;

        /// Allocates @a _Bytes bytes either into the stack if there is enough memory or delegates
        /// the allocation to the swap allocator.
        void* allocate(const size_t _Bytes, const uint32_t _Alignment) final;

        void deallocate(void* p, size_t bytes = 0u) final;

        /// Count of bytes available for stack allocations.
        constexpr size_t capacity() const { return stack_capacity; }

        /// Consumed bytes.
        size_t used() const
        {
            return m_UsedStackMemory;
        }

        /// Remaining bytes available for allocation.
        size_t space() const { return stack_capacity - m_UsedStackMemory; }

    private:
        uint8_t m_StackMemory[stack_capacity];
        size_t m_UsedStackMemory = 0u;
        SwapAllocator* m_pSwapAllocator;
    };

    template <size_t stack_capacity, class SwapAllocator>
    inline void* StackSwapAllocator<stack_capacity, SwapAllocator>::allocate(const size_t _Bytes, const uint32_t _Alignment)
    {
        // Try to allocate from the stack.
        if (m_UsedStackMemory < stack_capacity) {
            void* ptr = &m_StackMemory[m_UsedStackMemory];
            size_t space = stack_capacity - m_UsedStackMemory;

            if (nullptr != alignUp(_Alignment, _Bytes, ptr, space)) {
                // Successful stack allocation.
                m_UsedStackMemory = (uint8_t*)ptr - m_StackMemory + _Bytes;
                return ptr;
            }
        }

        // Stack is exhausted, allocate from swap.
        if (assertion(m_pSwapAllocator != nullptr,
                "Insufficient stack memory size to allocate %zu bytes (alignment is %zu), "
                "and swap allocations are disabled.",
                _Bytes, _Alignment)) {
            return m_pSwapAllocator->allocate(_Bytes, _Alignment);
        }

        return nullptr;
    }

    template <size_t stack_capacity, class SwapAllocator>
    inline void StackSwapAllocator<stack_capacity, SwapAllocator>::deallocate(void* p, size_t bytes)
    {
        if (m_pSwapAllocator != nullptr && (p < &m_StackMemory[0] || p > &m_StackMemory[stack_capacity - 1u])) {
            deallocateIfExists(m_pSwapAllocator, p, bytes);
        }
    }
}
}
