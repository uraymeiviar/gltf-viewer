// Copyright © 2021 UX3D GmbH. All rights reserved.

#pragma once

#include "math/VectorTypes.h"
#include <list>
#include <vector>

namespace ux3d::utils {
class BucketGridStructure {
public:
    BucketGridStructure(double _normalizedSearchRadius); //0..1

    void addElement(uint32_t _id, math::vec3 _normalizedPosition);
    std::list<uint32_t> getElements(math::vec3 _normalizedPosition);

private:
    std::vector<std::list<uint32_t>> m_buckets;

    math::ivec3 m_resolution = math::ivec3(1, 1, 1); //one big bucket

    std::list<uint32_t> getElements(math::ivec3 _position);
    void addElements(math::ivec3 _position, std::list<uint32_t>& _elements);
    uint32_t getIdx(math::ivec3 _position) const;
    math::ivec3 denormalize(math::vec3 _normalizedPosition) const;
    uint32_t getIdx(math::vec3 _normalizedPosition) const;

    const uint32_t m_maxGridResolution = 100u;
};
}
