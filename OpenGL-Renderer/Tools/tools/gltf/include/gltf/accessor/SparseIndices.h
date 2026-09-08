#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "gltf/GlTypes.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"

#include <optional>

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#reference-indices

namespace ux3d::gltf {
class BufferView;

class SparseIndices : public GltfObject {
private:
    BufferView* m_bufferView = nullptr;
    std::optional<uint32_t> m_byteOffset;
    ComponentType m_componentType = ComponentType::None;

public:
    explicit SparseIndices() = default;
    SparseIndices(SparseIndices&&) = default;
    SparseIndices(const SparseIndices& _source);

    BufferView* getBufferView() { return m_bufferView; }
    const BufferView* getBufferView() const { return m_bufferView; }
    GltfResult setBufferView(BufferView& _bufferView);
    void unsetBufferView() { m_bufferView = nullptr; }

    std::optional<uint32_t> getByteOffset() const { return m_byteOffset; }
    uint32_t getByteOffsetOrDefault() const { return m_byteOffset.value_or(0); }
    GltfResult setByteOffset(uint32_t _byteOffset);

    ComponentType getComponentType() const { return m_componentType; }
    GltfResult setComponentType(ComponentType _componentType);

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;
};
}
