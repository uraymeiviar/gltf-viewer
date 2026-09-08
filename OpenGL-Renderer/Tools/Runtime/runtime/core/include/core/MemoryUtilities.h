// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ux3d {
namespace core {

    /// @defgroup Alignment constants
    /// Constants to make code more readable.
    /// When using bare number literals, it may not be clear whether is expressed a size, offset, alignment or
    /// whatever.
    /// @{
    constexpr size_t ALIGNMENT_8_BYTES = 1u << 3u;
    constexpr size_t ALIGNMENT_16_BYTES = 1u << 4u;
    constexpr size_t ALIGNMENT_32_BYTES = 1u << 5u;
    constexpr size_t ALIGNMENT_64_BYTES = 1u << 6u;
    constexpr size_t ALIGNMENT_128_BYTES = 1u << 7u;
    constexpr size_t ALIGNMENT_256_BYTES = 1u << 8u;
    constexpr size_t ALIGNMENT_512_BYTES = 1u << 9u;
    constexpr size_t ALIGNMENT_1024_BYTES = 1u << 10u;
    /// @}

    /**
		 * Kilobyte unit.
		 */
    constexpr size_t operator"" _kB(unsigned long long int n)
    {
        return 1024 * n;
    }

    /**
		 * Megabyte unit.
		 */
    constexpr size_t operator"" _MB(unsigned long long int n)
    {
        return 1024 * 1024 * n;
    }

    enum class ByteMultiple : unsigned long long int {
        Kilobyte = 1000u,
        KB = Kilobyte,
        Megabyte = 1000u * 1000u,
        MB = Megabyte,
        Gigabyte = 1000u * 1000u * 1000u,
        GB = Gigabyte,

        Kibibyte = 1024u,
        KiB = Kibibyte,
        Mebibyte = 1024u * 1024u,
        MiB = Mebibyte,
        Gibibyte = 1024u * 1024u * 1024u,
        GiB = Gibibyte
    };

    constexpr size_t operator*(const unsigned long long int n, const ByteMultiple multiplier)
    {
        return n * static_cast<unsigned long long int>(multiplier);
    }

    constexpr uint32_t DefaultAlignment = 1u;

    /**
		 * Allocates a memory chunk of [size] bytes on the heap.
		 * The start address will be a multiple of [alignment].
		 */

    void* allocateAligned(size_t size, size_t alignment = DefaultAlignment);

    /**
		 * De-allocates memory chunk pointed by [ptr].
		 * The memory is expected to be allocated with alignment, using [allocate_aligned].
		 */
    void freeAligned(void* ptr);

    /**
		 * Returns true if n ∈ { 2^k | k ∈ ℕ }.
		 */
    bool isPowerOfTwo(size_t n);

    /**
		 * Returns max { k * alignment | k ∈ ℕ ∧ k * alignment <= pointer }.
		 * Down-aligns the give pointer to the given alignment.
		 * The alignment is required to be a power of two.
		 */

    void* alignDown(size_t alignment, const void* pAddress);

    /**
		 * This function tries to fit @a size bytes of storage with alignment
		 *  @a alignment into the buffer @a ptr of size @a space bytes.  If such
		 *  a buffer fits then @a ptr is changed to point to the first byte of the
		 *  aligned storage and @a space is reduced by the bytes used for alignment.
		 *
		 * Otherwise, ptr stays unchanged and nullptr is returned.
		 *
		 * @return @a ptr moved to aligned address, or nullptr on failure.
		 */

    void* alignUp(
        const size_t alignment,
        const size_t size,
        void*& ptr,
        size_t& space);

    void initHeapDebugging();
}

} // !ux3d
