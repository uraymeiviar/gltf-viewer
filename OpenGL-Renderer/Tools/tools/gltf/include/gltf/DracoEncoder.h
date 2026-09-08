#pragma once

#include "gltf/GltfResult.h"

#include <cstddef>

namespace ux3d::gltf {
class Gltf;
class Primitive;

class DracoEncoder {

    /// Sequential encoding ensures triangle order preservation.
    enum DracoMeshEncoderMethod {
        SEQUENTIAL_ENCODING = 0,
        EDGEBREAKER_ENCODING,
    };

public:
    struct QuantizationBits {
        int position = 14;
        int normal = 10;
        int uv = 12;
        int color = 12;
        int generic = 12;
    };

    /// Creates a Draco encoder.
    /// glTF objects creating during encoding such as buffers and buffer views
    /// are stored within the given glTF.
    /// Therefore only primitives residing within that same glTF should be encoded using this instance.
    DracoEncoder(Gltf& gltf);

    /// Encodes the given primitive with the current settings.
    /// The primitive itself is modified during the process,
    /// i.e. the compressed pirmitive is not a new glTF primitive.
    GltfResult encodePrimitive(Primitive& primitive);

    /// If triangle order can be changed, better compression results may be achieved.
    void setPreserveTriangleOrder(bool enabled);

    /// Sets the compression level in [0, 10] used while encoding.
    void setCompressionLevel(int compressionLevel);

    /// Sets the quantization bits used while encoding.
    void setQuantizationBits(const QuantizationBits& quantizationBits);

    void setLogOutput(bool enabled) { m_logOutputEnabled = enabled; }

private:
    Gltf& m_gltf;
    int m_targetCompressionLevel = 5; /// Value between 0 (best compression, slow) and 10 (bad compression, fast)
    DracoMeshEncoderMethod m_targetMeshEncoderMethod = SEQUENTIAL_ENCODING;
    QuantizationBits m_quantizationBits;
    bool m_logOutputEnabled = true;
};
}
