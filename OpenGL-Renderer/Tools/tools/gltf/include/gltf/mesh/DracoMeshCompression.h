// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "gltf/DracoEncoder.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"

namespace ux3d::gltf {
class BufferView;
class DracoMeshCompression : public GltfObject {
public:
    struct DracoAttribute {
        core::string name;
        uint32_t id;
    };

    core::Vector<DracoAttribute>& getAttributes() { return m_Attributes; }
    const core::Vector<DracoAttribute>& getAttributes() const { return m_Attributes; }

    BufferView* getBufferView() { return m_pBufferView; }
    const BufferView* getBufferView() const { return m_pBufferView; }
    GltfResult setBufferView(BufferView& _bufferView)
    {
        m_pBufferView = &_bufferView;
        return GltfResult::Success;
    };
    void unsetBufferView() { m_pBufferView = nullptr; }

private:
    core::Vector<DracoAttribute> m_Attributes;
    BufferView* m_pBufferView = nullptr;
};
}
