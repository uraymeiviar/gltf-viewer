// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Allocator.h"
#include <new> // placement new
#include <utility>

namespace ux3d {
/**
	 * @class LinearAllocator
	 * @brief A linear allocator with the ability to host child arenas.
	 * This allows for a recursively, scoped organized memory.
	 *
	 * @author Fabian Wahlster
	 */
namespace core {
    class LinearAllocator : public IAllocator {
    public:
        // default / invalid LinearAllocator
        LinearAllocator() = default;

        // LinearAllocator allocates a chunk of heap memory that is used for linear allocations
        explicit LinearAllocator(size_t _TotalByteSize, uint32_t _Alignment = DefaultAlignment);

        // Initialize with external memory, will not be freed on destruction
        LinearAllocator(void* _pData, size_t _TotalByteSize);

        // Frees memory that was allocated internally
        virtual ~LinearAllocator();

        LinearAllocator(const LinearAllocator&) = delete;
        LinearAllocator(LinearAllocator&& _Other) noexcept;

        LinearAllocator& operator=(const LinearAllocator&) = delete;
        LinearAllocator& operator=(LinearAllocator&&) = delete;

        // creates a non-owning child LinearAllocator hosted in this parent arena
        LinearAllocator child(size_t _TotalByteSize, uint32_t _Alignment = DefaultAlignment);

        // return the current free address, increment allocation offset
        void* allocate(size_t _ByteSize, uint32_t _Alignment = DefaultAlignment) final;

        // resize with a new allocation, previous allocation will be freed and all pointers aquired with allocate are invalid after this operation
        // allocation counter will be reset just as with calling reset(). Arena can only resize if it owns memory (m_ExternalData == false)
        // if the new allocation fails, the previous allocation will not be freed and stays intact
        // returns false on failure.
        bool resize(size_t _TotalByteSize, uint32_t _Alignment = DefaultAlignment);

        // reset the allocation to 0, no deallocation takes place
        void reset();

        // allocates a block of size matching T
        template <class T, size_t Alignment = alignof(T)>
        T* newBlock();

        // allocates a block of size matching T and calls the constructor with _Args
        template <class T, size_t Alignment = alignof(T), class... Args>
        T* newObject(Args&&... _Args);

        // check if the arena is valid and can allocate memory
        inline explicit operator bool() const { return m_pData != nullptr && m_Allocated < m_TotalSize; }

    private:
        size_t m_TotalSize = 0u;
        size_t m_Allocated = 0u; // TODO: use atomic for thread save allocation

        void* m_pData = nullptr;
        bool m_ExternalData = false;
    };

    template <class T, size_t Alignment>
    inline T* LinearAllocator::newBlock()
    {
        return reinterpret_cast<T*>(allocate(sizeof(T), Alignment));
    }

    template <class T, size_t Alignment, class... Args>
    inline T* LinearAllocator::newObject(Args&&... _Args)
    {
        return new (allocate(sizeof(T), Alignment)) T(std::forward<Args>(_Args)...);
    }

    /// Creates a new LinearAllocator acting upon a given array.
    /// The arena won't own the array, unlike the LinearStackAllocator.
    template <class T, size_t N>
    inline LinearAllocator makeLinearAllocator(T (&array)[N])
    {
        return LinearAllocator(&array[0], sizeof(T[N]));
    }
} // !core
} // !ux3d
