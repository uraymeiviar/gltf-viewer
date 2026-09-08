// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include <stdint.h>

namespace ux3d::gltf {
enum class StorageFormat : uint32_t
// accessors and images fall back to StorageFormat of related Buffer
{ // Association of names and value is used for ImageProperties and AccessorProperties. Do not change!
    BufferView = 0,
    File = 1,
    Embedded = 2,
    NotStored = 3,
    Undefined = 4
};

core::string storageFormatToString(StorageFormat _format);
StorageFormat storageFormatFromString(const core::string& _formatString);

enum class ExportFormatConfiguration : uint32_t {
    Undefined,
    SeparateFiles, // all images in separate files, all buffers in one file
    Embedded, // all images in base64 (separately), all buffers in one buffer encoded in base64
    CompleteMerge // all images and all buffers in one file
};
}
