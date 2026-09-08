// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once
#include "core/Result.h"

namespace ux3d {
namespace filesystem {
    enum class PathErrorE : uint32_t {
        /// File/directory not found or a path component used as a directory is actually a file.
        NotFound = core::DerivedResultStart,

        AccessDenied,

        /// Path string or name of file/directory too long.
        InvalidName,

        /// The system is unable to open the file, e.g. the limit of open files has been reached.
        SystemError,

        // Path was used to open a directory, but does not point to a directory
        PathIsNotADirectory,

        OutOfMemory,
    };

    using PathResult = core::ResultT<PathErrorE>;
}
}
