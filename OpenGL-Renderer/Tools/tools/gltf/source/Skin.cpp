// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Skin.h"
#include "gltf/Accessor.h"
#include "gltf/Node.h"

ux3d::gltf::Skin::Skin(uint32_t _id)
    : ChildOfRoot(_id)
{
}

ux3d::gltf::Skin::Skin(uint32_t _id, const ux3d::gltf::Skin& _source)
    : ChildOfRoot(_id, _source)
    , m_pInverseBindMatricesAccessor(_source.m_pInverseBindMatricesAccessor)
    , m_pSkeletonRoot(_source.m_pSkeletonRoot)
    , m_Joints(_source.m_Joints)
{
}

ux3d::gltf::GltfResult ux3d::gltf::Skin::setInverseBindMatricesAccessor(Accessor& _Accessor)
{
    m_pInverseBindMatricesAccessor = &_Accessor;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Skin::setSkeletonRoot(Node& _Node)
{
    m_pSkeletonRoot = &_Node;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Skin::addJoint(Node& _Joint)
{
    if (!m_Joints.push_back(&_Joint)) {
        return GltfError::AllocationFailure;
    }
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Skin::removeJoint(Node& _Joint)
{
    for (auto jointIter = m_Joints.begin(), end = m_Joints.end(); jointIter != end; ++jointIter) {
        if (*jointIter == &_Joint) {
            m_Joints.erase_keep_order(jointIter);
            return GltfResult::Success;
        }
    }

    return GltfError::ObjectNotFound;
}

bool ux3d::gltf::Skin::isValid() const
{

    if (m_Joints.empty()) {
        return false;
    } else {
        if (m_pInverseBindMatricesAccessor != nullptr) {
            //The array length must be the same as the count property of the inverseBindMatrices accessor when defined.
            if (m_pInverseBindMatricesAccessor->getCount() != m_Joints.size()) {
                return false;
            }
        }
    }
    return true;
}

ux3d::gltf::GltfResult ux3d::gltf::Skin::getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const
{
    if (_InvalidProperties.reserve(2) == false) {
        return GltfError::AllocationFailure;
    }

    if (m_Joints.empty()) {
        (void)_InvalidProperties.push_back("joints");
    } else {
        if (m_pInverseBindMatricesAccessor != nullptr) {
            // The array length must be the same as the count property of the inverseBindMatrices accessor when defined.
            if (m_pInverseBindMatricesAccessor->getCount() == m_Joints.size()) {
                (void)_InvalidProperties.push_back("inverseBindMatrices");
            }
        }
    }
    return GltfResult::Success;
}
