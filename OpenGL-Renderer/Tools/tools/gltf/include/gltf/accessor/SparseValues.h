#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"

#include <optional>

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#reference-values

namespace ux3d::gltf {
class BufferView;

class SparseValues : public GltfObject {
private:
    BufferView* m_bufferView = nullptr;
    std::optional<uint32_t> m_byteOffset;

public:
    explicit SparseValues() = default;
    SparseValues(SparseValues&&) = default;
    SparseValues(const SparseValues& _source);

    BufferView* getBufferView() { return m_bufferView; }
    const BufferView* getBufferView() const { return m_bufferView; }
    GltfResult setBufferView(BufferView& _bufferView);
    void unsetBufferView() { m_bufferView = nullptr; }

    std::optional<uint32_t> getByteOffset() const { return m_byteOffset; }
    uint32_t getByteOffsetOrDefault() const { return m_byteOffset.value_or(0); }
    GltfResult setByteOffset(uint32_t _byteOffset);

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;
};
}
