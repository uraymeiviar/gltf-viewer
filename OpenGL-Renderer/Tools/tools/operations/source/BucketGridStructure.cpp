// Copyright © 2022 UX3D GmbH. All rights reserved.

#include "BucketGridStructure.h"
#include <gtx/component_wise.hpp>
#include <utility>

using namespace ux3d;
using namespace ux3d::operations;
using namespace ux3d::operations::mesh;

BucketGridStructure::BucketGridStructure(double _normalizedSearchRadius)
{
    uint32_t gridResolution = std::max(1u, std::min(m_maxGridResolution, uint32_t(1.0 / _normalizedSearchRadius)));
    // Grid resolution is dependent on search radius as we are only considering neighboring buckets to find elements
    // e.g.
    // 5% => 0.05 => 20x20x20
    // 1% => 0.01 => 100x100x100
    if (_normalizedSearchRadius <= 0.0) {
        gridResolution = m_maxGridResolution;
    }

    m_resolution = glm::ivec3(gridResolution);
    m_buckets.resize(m_resolution.x * m_resolution.y * m_resolution.z);
}

void BucketGridStructure::addElement(uint32_t _id, glm::vec3 _normalizedPosition)
{
    uint32_t bucketIndex = getIdx(_normalizedPosition);
    m_buckets[bucketIndex].push_back(_id);
}

std::list<uint32_t> BucketGridStructure::getElements(glm::vec3 _normalizedPosition)
{
    return getElements(denormalize(_normalizedPosition));
}

std::list<uint32_t> BucketGridStructure::getElements(glm::ivec3 _position)
{
    std::list<uint32_t> elements;

    glm::ivec3 i = glm::ivec3(0);
    for (i.x = -1; i.x <= 1; i.x++)
        for (i.y = -1; i.y <= 1; i.y++)
            for (i.z = -1; i.z <= 1; i.z++) {
                addElements(_position + i, elements);
            }

    return elements;
}

void BucketGridStructure::addElements(glm::ivec3 _position, std::list<uint32_t>& _elements)
{
    if (_position.x < 0 || _position.y < 0 || _position.z < 0) {
        return;
    }
    if (_position.x >= m_resolution.x || _position.y >= m_resolution.y || _position.z >= m_resolution.z) {
        return;
    }

    uint32_t bucketIndex = getIdx(_position);

    // copy all bucket elements as we need to reuse the bucket
    _elements.insert(_elements.end(), m_buckets[bucketIndex].begin(), m_buckets[bucketIndex].end());
}

uint32_t BucketGridStructure::getIdx(glm::vec3 _normalizedPosition) const
{
    return getIdx(denormalize(_normalizedPosition));
}

uint32_t BucketGridStructure::getIdx(glm::ivec3 _position) const
{
    return _position.x + _position.y * m_resolution.x + _position.z * m_resolution.x * m_resolution.y;
}

glm::ivec3 BucketGridStructure::denormalize(glm::vec3 _normalizedPosition) const
{
    glm::ivec3 position = glm::ivec3(_normalizedPosition * glm::vec3(m_resolution - 1));
    position = glm::max(position, glm::ivec3(0));
    position = glm::min(position, m_resolution - 1);

    return position;
}
