// Copyright © 2021 UX3D GmbH. All rights reserved.

#include "utils/BucketGridStructure.h"
#include <gtx/component_wise.hpp>
#include <utility>

using namespace ux3d;
using namespace ux3d::utils;

ux3d::utils::BucketGridStructure::BucketGridStructure(double _normalizedSearchRadius)
{
    uint32_t gridResolution = std::max(1u, std::min(m_maxGridResolution, uint32_t(1.0 / _normalizedSearchRadius)));
    // Grid resolution is dependent on search radius as we are only considering neighboring buckets to find elements
    // e.g.
    // 5% => 0.05 => 20x20x20
    // 1% => 0.01 => 100x100x100
    if (_normalizedSearchRadius <= 0.0) {
        gridResolution = m_maxGridResolution;
    }

    m_resolution = math::ivec3(gridResolution);
    m_buckets.resize(m_resolution.x * m_resolution.y * m_resolution.z);
}

void ux3d::utils::BucketGridStructure::addElement(uint32_t _id, math::vec3 _normalizedPosition)
{
    uint32_t bucketIndex = getIdx(_normalizedPosition);
    m_buckets[bucketIndex].push_back(_id);
}

std::list<uint32_t> ux3d::utils::BucketGridStructure::getElements(math::vec3 _normalizedPosition)
{
    return getElements(denormalize(_normalizedPosition));
}

std::list<uint32_t> ux3d::utils::BucketGridStructure::getElements(math::ivec3 _position)
{
    std::list<uint32_t> elements;

    math::ivec3 i = math::ivec3(0);
    for (i.x = -1; i.x <= 1; i.x++)
        for (i.y = -1; i.y <= 1; i.y++)
            for (i.z = -1; i.z <= 1; i.z++) {
                addElements(_position + i, elements);
            }

    return elements;
}

void ux3d::utils::BucketGridStructure::addElements(math::ivec3 _position, std::list<uint32_t>& _elements)
{
    if (_position.x < 0 || _position.y < 0 || _position.z < 0) {
        return;
    }
    if (_position.x >= m_resolution.x || _position.y >= m_resolution.y || _position.z >= m_resolution.z) {
        return;
    }

    uint32_t bucketIndex = getIdx(_position);

    //copy all bucket elements as we need to reuse the bucket
    _elements.insert(_elements.end(), m_buckets[bucketIndex].begin(), m_buckets[bucketIndex].end());
}

uint32_t ux3d::utils::BucketGridStructure::getIdx(math::vec3 _normalizedPosition) const
{
    return getIdx(denormalize(_normalizedPosition));
}

uint32_t ux3d::utils::BucketGridStructure::getIdx(math::ivec3 _position) const
{
    return _position.x + _position.y * m_resolution.x + _position.z * m_resolution.x * m_resolution.y;
}

math::ivec3 ux3d::utils::BucketGridStructure::denormalize(math::vec3 _normalizedPosition) const
{
    math::ivec3 position = math::ivec3(_normalizedPosition * math::vec3(m_resolution - 1));
    position = glm::max(position, math::ivec3(0));
    position = glm::min(position, m_resolution - 1);

    return position;
}
