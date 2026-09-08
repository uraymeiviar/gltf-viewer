// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "filesystem/FileCopy.h"

ux3d::filesystem::FileCopyResult ux3d::filesystem::copyBinaryFile(const filesystem::File& _inputFile, const filesystem::File& _outputFile)
{
    if (_inputFile == _outputFile) {
        return FileCopyError::InputEqualsOutput;
    }

    core::Vector<uint8_t> data;
    if (FileLoadResult result = _inputFile.load(data);
        result != FileLoadResult::Success) {
        FileLoadResultE error = result;

        switch (error) {
        case (FileLoadResultE::InvalidPath):
            [[fallthrough]];
        case (FileLoadResultE::FileNotFound):
            return FileCopyError::InputFileNotFound;

        case (FileLoadResultE::AllocationFailed):
            return FileCopyError::AllocationFailure;

        case (FileLoadResultE::InsufficentPermissions):
            [[fallthrough]];
        case (FileLoadResultE::CannotOpenFile):
            [[fallthrough]];
        default:
            return FileCopyError::FileLoadError;
        }
    }

    if (FileSaveResult result = _outputFile.save(data);
        core::Failed(result)) {
        FileSaveResultE error = result;

        switch (error) {
        case (FileSaveResultE::InvalidPath):
            [[fallthrough]];
        case (FileSaveResultE::FileNotFound):
            return FileCopyError::OutputDirectoryNotFound;

        case (FileSaveResultE::InsufficentPermissions):
            [[fallthrough]];
        case (FileSaveResultE::CannotOpenFile):
            [[fallthrough]];
        default:
            return FileCopyError::FileWriteError;
        }
    }

    return FileCopyResult::Success;
}

ux3d::filesystem::FileCopyResult ux3d::filesystem::copyTextFile(const filesystem::File& _inputFile, const filesystem::File& _outputFile)
{
    if (_inputFile == _outputFile) {
        return FileCopyError::InputEqualsOutput;
    }

    core::string text;
    if (FileLoadResult result = _inputFile.load(text);
        result != FileLoadResult::Success) {
        FileLoadResultE error = result;

        switch (error) {
        case (FileLoadResultE::InvalidPath):
            [[fallthrough]];
        case (FileLoadResultE::FileNotFound):
            return FileCopyError::InputFileNotFound;

        case (FileLoadResultE::AllocationFailed):
            return FileCopyError::AllocationFailure;

        case (FileLoadResultE::InsufficentPermissions):
            [[fallthrough]];
        case (FileLoadResultE::CannotOpenFile):
            [[fallthrough]];
        default:
            return FileCopyError::FileLoadError;
        }
    }

    if (FileSaveResult result = _outputFile.save(text);
        result != FileSaveResult::Success) {
        FileSaveResultE error = result;

        switch (error) {
        case (FileSaveResultE::InvalidPath):
            [[fallthrough]];
        case (FileSaveResultE::FileNotFound):
            return FileCopyError::OutputDirectoryNotFound;

        case (FileSaveResultE::InsufficentPermissions):
            [[fallthrough]];
        case (FileSaveResultE::CannotOpenFile):
            [[fallthrough]];
        default:
            return FileCopyError::FileWriteError;
        }
    }

    return FileCopyResult::Success;
}
