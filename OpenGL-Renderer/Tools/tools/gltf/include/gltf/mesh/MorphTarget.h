// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "gltf/GltfResult.h"
#include <string>
#include <unordered_map>

namespace ux3d::gltf {
class Gltf;
class Accessor;

class MorphTarget {
    Accessor* m_PositionAccessor = nullptr;
    Accessor* m_NormalAccessor = nullptr;
    Accessor* m_TangentAccessor = nullptr;
    Accessor* m_TexCoord0Accessor = nullptr;
    Accessor* m_TexCoord1Accessor = nullptr;
    Accessor* m_Color0Accessor = nullptr;

public:
    MorphTarget(MorphTarget&&) = delete;

    explicit MorphTarget() { }

    MorphTarget(const MorphTarget&) = default;

    Accessor* getPositionAccessor() { return m_PositionAccessor; }
    const Accessor* getPositionAccessor() const { return m_PositionAccessor; }
    GltfResult setPositionAccessor(Accessor& _PositionAccessor);
    void unsetPositionAccessor() { m_PositionAccessor = nullptr; }

    Accessor* getNormalAccessor() { return m_NormalAccessor; }
    const Accessor* getNormalAccessor() const { return m_NormalAccessor; }
    GltfResult setNormalAccessor(Accessor& _NormalAccessor);
    void unsetNormalAccessor() { m_NormalAccessor = nullptr; }

    Accessor* getTangentAccessor() { return m_TangentAccessor; }
    const Accessor* getTangentAccessor() const { return m_TangentAccessor; }
    GltfResult setTangentAccessor(Accessor& _TangentAccessor);
    void unsetTangentAccessor() { m_TangentAccessor = nullptr; }

    Accessor* getTexCoord0Accessor() { return m_TexCoord0Accessor; }
    const Accessor* getTexCoord0Accessor() const { return m_TexCoord0Accessor; }
    GltfResult setTexCoord0Accessor(Accessor& _TexCoord0Accessor);
    void unsetTexCoord0Accessor() { m_TexCoord0Accessor = nullptr; }

    Accessor* getTexCoord1Accessor() { return m_TexCoord1Accessor; }
    const Accessor* getTexCoord1Accessor() const { return m_TexCoord1Accessor; }
    GltfResult setTexCoord1Accessor(Accessor& _TexCoord1Accessor);
    void unsetTexCoord1Accessor() { m_TexCoord1Accessor = nullptr; }

    Accessor* getColor0Accessor() { return m_Color0Accessor; }
    const Accessor* getColor0Accessor() const { return m_Color0Accessor; }
    GltfResult setColor0Accessor(Accessor& _Color0Accessor);
    void unsetColor0Accessor() { m_Color0Accessor = nullptr; }

    size_t accessorCount() const;
    std::unordered_map<std::string, Accessor*> getAccessorMapping() const;
};
}
