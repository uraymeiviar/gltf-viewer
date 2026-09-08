// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Light.h"

ux3d::gltf::Light::Light(uint32_t _id, Type _type)
    : ChildOfRoot(_id)
    , m_type(_type)
{
    if (m_type == Type::Spot) {
        m_spot.emplace();
    }
}

ux3d::gltf::Light::Light(uint32_t _id, const Light& _source)
    : ChildOfRoot(_id, _source)
    , m_color(_source.m_color)
    , m_intensity(_source.m_intensity)
    , m_range(_source.m_range)
    , m_type(_source.m_type)
{
    if (_source.m_spot.has_value()) {
        m_spot.emplace(_source.m_spot.value());
    }
}

ux3d::gltf::GltfResult ux3d::gltf::Light::Spot::setInnerConeAngle(double _innerConeAngle)
{
    if (_innerConeAngle < 0 || _innerConeAngle >= math::half_pi<double>() || (hasOuterConeAngle() && _innerConeAngle >= getOuterConeAngleOrDefault())) {
        return GltfError::InvalidInput;
    }
    m_innerConeAngle = _innerConeAngle;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Light::Spot::setOuterConeAngle(double _outerConeAngle)
{
    if (_outerConeAngle <= 0 || _outerConeAngle > math::half_pi<double>() || (hasInnerConeAngle() && _outerConeAngle <= getInnerConeAngleOrDefault())) {
        return GltfError::InvalidInput;
    }
    m_outerConeAngle = _outerConeAngle;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Light::setColor(const math::dvec3& _color)
{
    for (int i = 0; i < _color.length(); ++i) {
        if (_color[i] < 0 || _color[i] > 1) {
            return GltfError::InvalidInput;
        }
    }
    m_color = _color;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Light::setIntensity(const double _intensity)
{
    if (_intensity < 0) {
        return GltfError::InvalidInput;
    }
    m_intensity = _intensity;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Light::setRange(const double _range)
{

    m_range = _range;
    if (m_range <= 0) {
        m_range.reset();
    }
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Light::setType(const Type& _type)
{
    m_type = _type;

    if (m_type == Type::Spot && m_spot.has_value() == false) {
        m_spot.emplace();
    }

    return GltfResult::Success;
}

bool ux3d::gltf::Light::isValid() const
{
    if (m_type == Light::Type::Spot) {
        return m_spot.has_value();
    }
    return true;
}

ux3d::gltf::GltfResult ux3d::gltf::Light::getInvalidProperties(core::Vector<core::string>& _invalidProperties) const
{
    if (false == _invalidProperties.reserve(1)) {
        return GltfError::AllocationFailure;
    }

    if (m_type == Light::Type::Spot) {
        if (false == m_spot.has_value()) {
            (void)(_invalidProperties.emplace_back("spot"));
        }
    }

    return GltfResult::Success;
}

void ux3d::gltf::Light::destroySpot()
{
    m_spot.reset();
}

void ux3d::gltf::Light::createSpot()
{
    if (m_spot.has_value() == false) {
        m_spot.emplace();
    }
}
