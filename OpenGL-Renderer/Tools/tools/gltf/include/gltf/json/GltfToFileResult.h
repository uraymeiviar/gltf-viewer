// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Result.h"

namespace ux3d::gltf {
enum class GltfToFileError : uint32_t {
    AllocationFailure = core::DerivedResultStart,
    UnsupportedFileFormat,
    OpenDirectoryFailure,
    SaveFileFailure,
    JsonFailure,
    ResourceFileAlreadyExists
};
using GltfToFileResult = core::ResultT<GltfToFileError>;

}
