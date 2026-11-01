// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "gltf/MeshoptCompression.h"
#include "core/LoadProgress.h"
#include "core/Logger.h"
#include "core/Platform.h"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/Gltf.h"

UX3D_BEGIN_IGNORE_WARNINGS
#include <meshoptimizer.h>
UX3D_END_IGNORE_WARNINGS

#include <cstdlib>
#include <cstring>
#include <string>

using namespace ux3d;
using namespace ux3d::gltf;

bool ux3d::gltf::meshopt::decodingDisabled()
{
    static const bool disabled = [] {
        const char* value = std::getenv("GLIDER_TEST_DISABLE_MESHOPT");
        return value != nullptr && value[0] != '\0' && std::string(value) != "0";
    }();
    return disabled;
}

namespace {

const char* modeName(MeshoptCompression::Mode mode)
{
    switch (mode) {
    case MeshoptCompression::Mode::Attributes: return "ATTRIBUTES";
    case MeshoptCompression::Mode::Triangles: return "TRIANGLES";
    case MeshoptCompression::Mode::Indices: return "INDICES";
    }
    return "?";
}

// The checks of the extension specification for one bufferView, then the decode straight into the view's range of
// its own buffer. Returns an empty string on success, otherwise why it failed.
std::string decodeView(BufferView& view)
{
    const MeshoptCompression& info = *view.getMeshoptCompression();
    using Mode = MeshoptCompression::Mode;
    using Filter = MeshoptCompression::Filter;
    const bool khr = info.extension == meshopt::KhrName;

    if (info.buffer == nullptr) {
        return "the compressed buffer index is invalid";
    }
    if (view.getBuffer() == nullptr) {
        return "the bufferView has no buffer to decode into";
    }
    const uint64_t decodedSize = uint64_t(info.count) * info.byteStride;
    if (decodedSize > view.getByteLength()) {
        return "count x byteStride (" + std::to_string(decodedSize) + ") exceeds the bufferView byteLength ("
            + std::to_string(view.getByteLength()) + ")";
    }
    if (view.getByteStride().has_value() && info.mode == Mode::Attributes && *view.getByteStride() != info.byteStride) {
        return "byteStride differs from the bufferView byteStride";
    }
    switch (info.mode) {
    case Mode::Attributes:
        if (info.byteStride == 0 || info.byteStride % 4 != 0 || info.byteStride > 256) {
            return "ATTRIBUTES byteStride must be a multiple of 4 up to 256";
        }
        break;
    case Mode::Triangles:
        if (info.count % 3 != 0) {
            return "TRIANGLES count must be a multiple of 3";
        }
        [[fallthrough]];
    case Mode::Indices:
        if (info.byteStride != 2 && info.byteStride != 4) {
            return std::string(modeName(info.mode)) + " byteStride must be 2 or 4";
        }
        if (info.filter != Filter::None) {
            return std::string(modeName(info.mode)) + " does not take a filter";
        }
        break;
    }
    switch (info.filter) {
    case Filter::None:
        break;
    case Filter::Octahedral:
        if (info.byteStride != 4 && info.byteStride != 8) return "OCTAHEDRAL byteStride must be 4 or 8";
        break;
    case Filter::Quaternion:
        if (info.byteStride != 8) return "QUATERNION byteStride must be 8";
        break;
    case Filter::Exponential:
        break; // multiple of 4 (checked above)
    case Filter::Color:
        if (!khr) return "the COLOR filter is defined by KHR_meshopt_compression only";
        if (info.byteStride != 4 && info.byteStride != 8) return "COLOR byteStride must be 4 or 8";
        break;
    }

    core::MemoryRange<const uint8_t> source;
    if (core::Failed(info.buffer->getData(source)) || uint64_t(info.byteOffset) + info.byteLength > source.size()) {
        return "the compressed range [" + std::to_string(info.byteOffset) + ", +" + std::to_string(info.byteLength)
            + ") is outside its buffer";
    }
    const unsigned char* encoded = source.data() + info.byteOffset;

    uint8_t* destination = view.getBuffer()->mutableRange(view.getByteOffset(), size_t(decodedSize));
    if (destination == nullptr) {
        return "the bufferView range is outside its buffer";
    }
    if (info.count == 0) {
        return {};
    }

    int status = 0;
    switch (info.mode) {
    case Mode::Attributes:
        if (!khr && meshopt_decodeVertexVersion(encoded, info.byteLength) > 0) {
            return "EXT_meshopt_compression allows vertex codec version 0 only";
        }
        status = meshopt_decodeVertexBuffer(destination, info.count, info.byteStride, encoded, info.byteLength);
        break;
    case Mode::Triangles:
        status = meshopt_decodeIndexBuffer(destination, info.count, info.byteStride, encoded, info.byteLength);
        break;
    case Mode::Indices:
        status = meshopt_decodeIndexSequence(destination, info.count, info.byteStride, encoded, info.byteLength);
        break;
    }
    if (status != 0) {
        std::memset(destination, 0, size_t(decodedSize)); // never leave half-decoded data behind
        return std::string(modeName(info.mode)) + " stream is invalid (meshoptimizer error " + std::to_string(status) + ")";
    }
    switch (info.filter) {
    case Filter::None: break;
    case Filter::Octahedral: meshopt_decodeFilterOct(destination, info.count, info.byteStride); break;
    case Filter::Quaternion: meshopt_decodeFilterQuat(destination, info.count, info.byteStride); break;
    case Filter::Exponential: meshopt_decodeFilterExp(destination, info.count, info.byteStride); break;
    case Filter::Color: meshopt_decodeFilterColor(destination, info.count, info.byteStride); break;
    }
    return {};
}

} // namespace

GltfResult ux3d::gltf::meshopt::decodeBufferViews(Gltf& gltf)
{
    // A switched-off meshopt extension (File > Settings > glTF Extensions, or the GLIDER_TEST_DISABLE_MESHOPT alias)
    // never reaches this point: GltfFromJson refuses a file that requires it and removes it from one that only uses
    // it, so no bufferView carries compression and the fallback buffers are read.
    GltfResult result;

    uint32_t index = 0;
    uint32_t failures = 0;
    for (BufferView& view : gltf.getBufferViews()) {
        if (view.getMeshoptCompression().has_value()) {
            const std::string error = decodeView(view);
            if (!error.empty()) {
                ++failures;
                core::logError("[Loading] %s: bufferView %u cannot be decoded: %s",
                    view.getMeshoptCompression()->extension.c_str(), index, error.c_str());
                result = core::Max(GltfResult(GltfError::MeshoptDecodingError), result);
            }
            ++core::loading::state.completed;
        }
        ++index;
    }
    if (failures != 0) {
        core::logError("[Loading] %u compressed bufferView(s) failed to decode; the model is not loaded rather than "
                       "shown with broken geometry", failures);
    }
    return result;
}
