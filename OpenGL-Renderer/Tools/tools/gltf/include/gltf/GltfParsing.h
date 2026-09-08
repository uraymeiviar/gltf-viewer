// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/HeapSlice.h"
#include "core/String.h"
#include "core/Vector.h"
#include "filesystem/Dir.h"
#include "gltf/GltfBinaryResolver.h"
#include "json/GltfFromFileResult.h"
#include "json/GltfToFileResult.h"
#include <optional>

namespace ux3d::filesystem {
class File;
}

namespace ux3d::gltf {
class Gltf;
class ChildOfRoot;
struct ExportOptions;
struct ImportOptions;

class GltfParsing {
public:
    static GltfFromFileResult fromFile(
        Gltf& _outGltf,
        const filesystem::File& _file,
        const ImportOptions& _importOptions,
        bool _isGizmo = true);

    static GltfToFileResult toFile(
        const Gltf& _gltf,
        const filesystem::File& _file,
        ExportOptions _exportOptions);

    static GltfFromFileResult fromString(
        Gltf& _outGltf,
        const filesystem::Dir& _gltfDir,
        const core::string& _gltfString,
        const ImportOptions& _importOptions);

private:
    static GltfFromFileResult fromGltfFile(
        Gltf& _outGltf,
        const filesystem::File& _file,
        const ImportOptions& _importOptions);

    static GltfFromFileResult fromGlbFile(
        Gltf& _outGltf,
        const filesystem::File& _file,
        const ImportOptions& _importOptions);

    static GltfFromFileResult fromBinary(
        Gltf& _OutGltf,
        const core::HeapSlice<uint8_t>& _data,
        const ImportOptions& _importOptions);

    static GltfToFileResult toGltfFile(
        const Gltf& _gltf,
        const filesystem::File& _file,
        const ExportOptions& _exportOptions);

    static GltfToFileResult toGlbFile(
        const Gltf& _gltf,
        const filesystem::File& _file,
        const ExportOptions& _exportOptions);
};
}
