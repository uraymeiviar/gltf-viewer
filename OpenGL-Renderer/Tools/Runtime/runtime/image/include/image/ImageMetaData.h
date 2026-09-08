#pragma once

#include <variant>

namespace ux3d::image {
enum class KtxCompressionScheme : uint32_t {
    UNDEFINED = 0,
    ETC1S = 163,
    UASTC = 166,
    UNKNOWN = 0xFFFFFFFF
};

enum class KtxSupercompressionScheme : uint32_t {
    NONE = 0, // UASTC
    BASISLZ = 1, // ETC1S
    ZSTANDARD = 2, // UASTC
    UNKNOWN = 0xFFFFFFFF
};

struct KTXMetaData {
    //ETC1S options
    uint32_t etc1sQualityLevel = 128;
    uint32_t etc1sCompressionLevel = 1;

    //UASTC options
    uint32_t uastcQualityLevel = 2;
    float uastcRDO = 1.0f;
    uint32_t zstdCompressionLevel = 3;

    KtxSupercompressionScheme supercompressionSchema = KtxSupercompressionScheme::UNKNOWN;
    KtxCompressionScheme colorModel = KtxCompressionScheme::UNKNOWN;
    uint8_t colorPrimaries = 0;
    uint8_t transferFunction = 0;
};

struct JPEGMetaData {
    uint32_t compressionRate = 100;
};

using ImageMetaData = std::variant<std::monostate, KTXMetaData, JPEGMetaData>;
}
