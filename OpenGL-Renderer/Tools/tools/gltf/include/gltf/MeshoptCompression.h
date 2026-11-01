// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/GltfResult.h"

#include <cstdint>
#include <string>
#include <string_view>

// EXT_meshopt_compression / KHR_meshopt_compression on a bufferView:
// https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/EXT_meshopt_compression
// https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_meshopt_compression
// The two are the same bitstream (KHR adds the vertex codec version 1 and the COLOR filter). The loader decodes
// the compressed range into the bufferView's own buffer range (Gltf::loadBinaryDataToMemory), so everything after
// loading sees a plain bufferView.

namespace ux3d::gltf {
class Buffer;
class Gltf;

struct MeshoptCompression {
    enum class Mode { Attributes, Triangles, Indices };
    enum class Filter { None, Octahedral, Quaternion, Exponential, Color };

    std::string extension; // "EXT_meshopt_compression" or "KHR_meshopt_compression"
    Buffer* buffer = nullptr; // the buffer holding the compressed stream (resolved from bufferIndex)
    int64_t bufferIndex = -1;
    uint32_t byteOffset = 0;
    uint32_t byteLength = 0;
    uint32_t byteStride = 0;
    uint32_t count = 0;
    Mode mode = Mode::Attributes;
    Filter filter = Filter::None;
};

namespace meshopt {
inline constexpr std::string_view ExtName = "EXT_meshopt_compression";
inline constexpr std::string_view KhrName = "KHR_meshopt_compression";
inline bool isMeshoptExtension(std::string_view name) { return name == ExtName || name == KhrName; }

// Test hook: GLIDER_TEST_DISABLE_MESHOPT=1 reads files as a loader without the extension would (the
// compressed bufferViews are left undecoded), so a file that requires it must fail with a clear error.
bool decodingDisabled();

// Decodes every compressed bufferView of a loaded model into its buffer range. Buffers whose
// EXT/KHR_meshopt_compression.fallback is true were allocated (zeroed) instead of being read. Returns
// GltfError::MeshoptDecodingError, after logging which bufferView failed and why, when a stream is invalid.
GltfResult decodeBufferViews(Gltf& gltf);
}
}
