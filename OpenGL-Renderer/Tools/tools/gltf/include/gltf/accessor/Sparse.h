#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/accessor/SparseIndices.h"
#include "gltf/accessor/SparseValues.h"

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#reference-sparse

namespace ux3d::gltf {
class Sparse : public GltfObject {
private:
    uint32_t m_count = 1u;
    SparseIndices m_indices;
    SparseValues m_values;

public:
    explicit Sparse() = default;
    Sparse(Sparse&&) = default;

    Sparse(const Sparse& _source);

    uint32_t getCount() const { return m_count; }
    GltfResult setCount(uint32_t _count);

    SparseIndices& getIndices() { return m_indices; }
    const SparseIndices& getIndices() const { return m_indices; }

    SparseValues& getValues() { return m_values; }
    const SparseValues& getValues() const { return m_values; }

    size_t getSizeInBytes(size_t _elementSize) const;

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;
};
}
