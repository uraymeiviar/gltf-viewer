// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class IndexLookup;

class GltfToJsonBase {
protected:
    const IndexLookup& m_Indices;

public:
    explicit GltfToJsonBase(const IndexLookup& _Indices)
        : m_Indices(_Indices)
    {
    }
};
}
