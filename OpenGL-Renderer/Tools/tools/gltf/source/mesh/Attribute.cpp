// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/mesh/Attribute.h"

bool ux3d::gltf::Attribute::isValid() const
{
    if (m_Name.empty()) {
        return false;
    }
    if (m_Accessor == nullptr) {
        return false;
    }
    return true;
}

ux3d::gltf::GltfResult ux3d::gltf::Attribute::setAccessor(Accessor* _Accessor)
{
    m_Accessor = _Accessor;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Attribute::setName(core::string _Name)
{
    m_Name = _Name;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Attribute::getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const
{
    if (m_Name.empty()) {
        if (_InvalidProperties.push_back("attributeName") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    if (m_Accessor == nullptr) {
        if (_InvalidProperties.push_back("attributeAccessor") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    return GltfResult::Success;
}
