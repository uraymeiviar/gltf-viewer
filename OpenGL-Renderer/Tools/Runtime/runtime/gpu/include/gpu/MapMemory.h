// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Memory.h
//  middleware
//
//  Created by Jim Eckerlein on 7.06.2019.
//

#pragma once

#include "Types.h"

namespace ux3d::gpu {
// forward decls:
struct GPUAllocator;
struct GPUAllocation;
struct Buffer;
struct Image;

/// Maps the memory backed by the given allocation from the given allocator.
/// The out-pointer is set to the location of the mapped memory.
GpuResult mapMemory(const GPUAllocator& _gpuAllocator, const GPUAllocation& _gpuAllocation, void*& _pOutMappedData);
GpuResult unmapMemory(const GPUAllocator& _gpuAllocator, const GPUAllocation& _gpuAllocation);

GpuResult mapMemory(const Buffer& _buffer, void*& _pOutMappedData);
GpuResult unmapMemory(const Buffer& _buffer);

GpuResult mapMemory(const Image& _image, void*& _pOutMappedData);
GpuResult unmapMemory(const Image& _image);

// Resource type is Buffer or Image
// MemoryCallback type is a functor taking a void* with the address to host visible memory returned by the driver upon successfull VMA mapMemory call
// Example usage:
//	gpu::mapMemory(bufferEmit.resource.buffer, [&bufferEmit](void* _pHostPtr)
//	{
//		memcpy(_pHostPtr, bufferEmit.description.srcBytes.data(), bufferEmit.description.srcBytes.size());
//	});

template <class Resource, class MemoryCallback>
GpuResult mapMemory(const Resource& _resource, MemoryCallback _callback)
{
    void* pHostMappedMemory = nullptr;
    GpuResult result = mapMemory(_resource, pHostMappedMemory);

    if (result) {
        _callback(pHostMappedMemory);
        result = unmapMemory(_resource);
    }

    return result;
}
}
