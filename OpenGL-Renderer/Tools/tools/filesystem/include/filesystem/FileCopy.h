// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Result.h"
#include "filesystem/File.h"

namespace ux3d::filesystem {
enum class FileCopyError : uint32_t {
    InputFileNotFound = core::DerivedResultStart,
    FileLoadError,
    AllocationFailure,
    OutputDirectoryNotFound,
    FileWriteError,
    InputEqualsOutput
};

using FileCopyResult = core::ResultT<FileCopyError>;

FileCopyResult copyBinaryFile(const filesystem::File& _inputFile, const filesystem::File& _outputFile);
FileCopyResult copyTextFile(const filesystem::File& _inputFile, const filesystem::File& _outputFile);
}
