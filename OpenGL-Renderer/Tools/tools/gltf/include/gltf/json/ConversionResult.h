// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Result.h"

namespace ux3d::gltf {
enum class ConversionError : uint32_t {
    SuccessWithErrors = core::DerivedResultStart,
    UnsupportedFileFormat,
    MissingProperty,
    FileNotFound,
    EmptyBuffer,
    ImageWriteFailure,
    InvalidType,
    InvalidArgument,
    InvalidVectorLength,
    InvalidFilePath,
    EnumMismatch,
    InvalidGltf,
    FileSaveFailure,
    InvalidJson,
    InvalidGlbFileLength,
    LoadDataToMemoryFailure,
    IndexOutOfRange,
    AllocationFailure,
    EncodeFailure,
    BufferInvalid,
    ResourceFileAlreadyExists
};

using ConversionResult = core::ResultT<ConversionError>;
}
