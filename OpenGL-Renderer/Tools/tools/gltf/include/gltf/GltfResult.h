// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Result.h"

namespace ux3d::gltf {
enum class GltfError : uint32_t {
    ObjectNotFound = core::DerivedResultStart,
    ObjectAlreadyExists,
    GltfRootMismatch,
    InvalidInput,
    NodeHasParent,
    SelfReference,
    HierarchyRecursion,
    AllocationFailure,
    NodeIsPartOfSkin,
    DracoDecodingError,
    DracoEncodingError,
    AttributeDeletionFailure,
    EncodingFailed,
    ResolutionMismatch,
    TextureTransformIsPresent,
    MeshoptDecodingError // a compressed bufferView (EXT/KHR_meshopt_compression) could not be decoded
};

using GltfResult = core::ResultT<GltfError>;
}
