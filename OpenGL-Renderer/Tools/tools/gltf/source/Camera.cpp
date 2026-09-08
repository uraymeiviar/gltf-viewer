// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Camera.h"

ux3d::gltf::Camera::Camera(uint32_t _id, ProjectionType _projectionType)
    : ChildOfRoot(_id)
    , m_Type(_projectionType)
{
    switch (m_Type) {
    case ProjectionType::Perspective:
        m_Perspective.emplace();
        break;
    case ProjectionType::Orthographic:
        m_Orthographic.emplace();
        break;
    }
}

ux3d::gltf::Camera::Camera(uint32_t _id, const Camera& _source)
    : ChildOfRoot(_id, _source)
    , m_Type(_source.m_Type)
    , m_Perspective(_source.m_Perspective)
    , m_Orthographic(_source.m_Orthographic)
{
}

ux3d::gltf::GltfResult ux3d::gltf::Camera::setType(const ProjectionType& type)
{
    m_Type = type;

    if (m_Type == ProjectionType::Orthographic && m_Orthographic.has_value() == false) {
        m_Orthographic.emplace();
        m_Perspective.reset();
    }
    if (m_Type == ProjectionType::Perspective && m_Perspective.has_value() == false) {
        m_Perspective.emplace();
        m_Orthographic.reset();
    }

    return GltfResult::Success;
}

bool ux3d::gltf::Camera::isValid() const
{
    if (m_Type == ProjectionType::Orthographic && m_Orthographic.has_value()) {
        return true;
    }
    if (m_Type == ProjectionType::Perspective && m_Perspective.has_value()) {
        return true;
    }
    return false;
}
ux3d::gltf::GltfResult ux3d::gltf::Camera::getInvalidProperties(core::Vector<ux3d::core::string>& _InvalidProperties) const
{
    if (m_Type == ProjectionType::Orthographic && !m_Orthographic.has_value()) {
        if (_InvalidProperties.push_back("orthographic") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    if (m_Type == ProjectionType::Perspective && !m_Perspective.has_value()) {
        if (_InvalidProperties.push_back("perspective") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    return GltfResult::Success;
}
