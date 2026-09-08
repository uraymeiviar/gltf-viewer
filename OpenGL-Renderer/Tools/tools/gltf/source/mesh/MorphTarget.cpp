// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/mesh/MorphTarget.h"
#include "gltf/Accessor.h"
#include "gltf/Mesh.h"
#include <cstddef>

using namespace ux3d::gltf;

GltfResult MorphTarget::setPositionAccessor(Accessor& _PositionAccessor)
{
    m_PositionAccessor = &_PositionAccessor;
    return GltfResult::Success;
}

GltfResult MorphTarget::setNormalAccessor(Accessor& _NormalAccessor)
{
    m_NormalAccessor = &_NormalAccessor;
    return GltfResult::Success;
}

GltfResult MorphTarget::setTangentAccessor(Accessor& _TangentAccessor)
{
    m_TangentAccessor = &_TangentAccessor;
    return GltfResult::Success;
}

GltfResult MorphTarget::setTexCoord0Accessor(Accessor& _TexCoord0Accessor)
{
    m_TexCoord0Accessor = &_TexCoord0Accessor;
    return GltfResult::Success;
}

GltfResult MorphTarget::setTexCoord1Accessor(Accessor& _TexCoord1Accessor)
{
    m_TexCoord1Accessor = &_TexCoord1Accessor;
    return GltfResult::Success;
}

GltfResult MorphTarget::setColor0Accessor(Accessor& _Color0Accessor)
{
    m_Color0Accessor = &_Color0Accessor;
    return GltfResult::Success;
}

size_t MorphTarget::accessorCount() const
{
    size_t count = 0;
    if (m_NormalAccessor != nullptr) {
        ++count;
    }
    if (m_PositionAccessor != nullptr) {
        ++count;
    }
    if (m_TangentAccessor != nullptr) {
        ++count;
    }
    if (m_TexCoord0Accessor != nullptr) {
        ++count;
    }
    if (m_TexCoord1Accessor != nullptr) {
        ++count;
    }
    if (m_Color0Accessor != nullptr) {
        ++count;
    }
    return count;
}

std::unordered_map<std::string, Accessor*> MorphTarget::getAccessorMapping() const
{
    std::unordered_map<std::string, Accessor*> result;
    if (m_NormalAccessor != nullptr) {
        result["NORMAL"] = m_NormalAccessor;
    }
    if (m_PositionAccessor != nullptr) {
        result["POSITION"] = m_PositionAccessor;
    }
    if (m_TangentAccessor != nullptr) {
        result["TANGENT"] = m_TangentAccessor;
    }
    if (m_TexCoord0Accessor != nullptr) {
        result["TEXCOORD_0"] = m_TexCoord0Accessor;
    }
    if (m_TexCoord1Accessor != nullptr) {
        result["TEXCOORD_1"] = m_TexCoord1Accessor;
    }
    if (m_Color0Accessor != nullptr) {
        result["COLOR_0"] = m_Color0Accessor;
    }
    return result;
}
