// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "core/MemoryUtilities.h"

#include "core/Logger.h"

#define UX3D_NO_WINDOWS_HEADER
#include "core/Platform.h"

// https://docs.microsoft.com/en-us/visualstudio/debugger/finding-memory-leaks-using-the-crt-library?view=vs-2015

#if !defined(UX3D_NO_DEBUG_HEAP) && defined(UX3D_IS_WINDOWS)
#define _CRTDBG_MAP_ALLOC
#endif

#include <stdlib.h>

#if !defined(UX3D_NO_DEBUG_HEAP) && defined(UX3D_IS_WINDOWS)

#include <crtdbg.h>

#endif

#include <errno.h>

bool ux3d::core::isPowerOfTwo(const size_t n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

static size_t twosComplement(size_t n)
{
    n = ~n;
    n += 1;
    return n;
}

void* ux3d::core::alignDown(const size_t _alignment, const void* const _ptr)
{
    if (assertion(isPowerOfTwo(_alignment), "Invalid _alignment %zu\n", _alignment) && assertion(_ptr != nullptr, "Cannot align null pointer")) {
        return reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(_ptr) & twosComplement(_alignment));
    }

    return nullptr;
}

void* ux3d::core::alignUp(const size_t _alignment, const size_t _size, void*& _ptr, size_t& _space)
{
    if (assertion(isPowerOfTwo(_alignment), "Invalid _alignment %zu\n", _alignment) && assertion(_ptr != nullptr, "Cannot align null pointer")) {
        const auto addr = reinterpret_cast<uintptr_t>(_ptr);
        const auto aligned_addr = (addr - 1u + _alignment) & twosComplement(_alignment);
        const auto diff = aligned_addr - addr;

        if (_size + diff <= _space) {
            _space -= diff;
            return (_ptr = reinterpret_cast<void*>(aligned_addr));
        }
    }

    return nullptr;
}

void* ux3d::core::allocateAligned(const size_t _size, const size_t _alignment)
{
    void* pData = nullptr;

#ifdef UX3D_IS_WINDOWS

    pData = _aligned_malloc(_size, _alignment);

#endif

#ifdef UX3D_IS_POSIX

    const int result = posix_memalign(&pData, _alignment < sizeof(void*) ? sizeof(void*) : _alignment, _size);

    if (result == 0) {
        return pData;
    } else {
        // explicitly check for failure reason
        if (result == ENOMEM) {
            (void)assertion(false, "Out of heap memory");
        } else if (result == EINVAL) {
            (void)assertion(false, "Alignment %d is not power of two", _alignment);
        }
    }

#endif

    if (assertion(pData != nullptr, "Allocation of %ull bytes with %ull-byte _alignment failed", _size, _alignment)) {
        return pData;
    }

    return nullptr;
}

void ux3d::core::freeAligned(void* _ptr)
{
#ifdef UX3D_IS_WINDOWS

    _aligned_free(_ptr);

#elif defined(UX3D_IS_POSIX)

    free(_ptr);
#else

    static_assert(false, "Aligned free not available on this platform");

#endif
}

void ux3d::core::initHeapDebugging()
{
#ifndef UX3D_NO_DEBUG_HEAP

#ifdef UX3D_IS_WINDOWS
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif

#endif // !UX3D_DEBUG_HEAP
}
