// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Result.h"

namespace ux3d::gltf {
enum class GltfFromFileError : uint32_t {
    UnsupportedFileFormat = core::DerivedResultStart,
    ReadFileFailure,
    OpenDirectoryFailure,
    InvalidJson,
    InvalidGltf,
    AllocationFailure
};
using GltfFromFileResult = core::ResultT<GltfFromFileError>;
}
