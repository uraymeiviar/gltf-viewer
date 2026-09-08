#pragma once

#include "core/Result.h"
#include "stdint.h"

namespace ux3d::filesystem {
enum class FileLoadResultE : uint32_t {
    InvalidPath = core::DerivedResultStart,
    FileNotFound,
    InsufficentPermissions,
    CannotOpenFile,
    AllocationFailed,
    SandboxError,
};
using FileLoadResult = core::ResultT<FileLoadResultE>;

enum class FileSaveResultE : uint32_t {
    InvalidPath = core::DerivedResultStart,
    FileNotFound,
    InsufficentPermissions,
    CannotOpenFile,
    SandboxError
};
using FileSaveResult = core::ResultT<FileSaveResultE>;
}
