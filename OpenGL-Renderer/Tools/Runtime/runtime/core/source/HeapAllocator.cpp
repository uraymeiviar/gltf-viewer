// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "core/HeapAllocator.h"
#include "core/LanguageFeatures.h"

void* ux3d::core::HeapAllocator::allocate(size_t n, uint32_t alignment)
{
    return allocateAligned(n, alignment);
}

void ux3d::core::HeapAllocator::deallocate(void* p, size_t n)
{
    (void)n;
    freeAligned(p);
}
