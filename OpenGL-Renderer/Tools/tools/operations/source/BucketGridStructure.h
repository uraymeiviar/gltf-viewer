// Copyright © 2021 UX3D GmbH. All rights reserved.

#pragma once

#include <glm.hpp>
#include <list>
#include <vector>

namespace ux3d::operations::mesh {
class BucketGridStructure {
public:
    BucketGridStructure(double _normalizedSearchRadius); // 0..1

    void addElement(uint32_t _id, glm::vec3 _normalizedPosition);
    std::list<uint32_t> getElements(glm::vec3 _normalizedPosition);

private:
    std::vector<std::list<uint32_t>> m_buckets;

    glm::ivec3 m_resolution = glm::ivec3(1, 1, 1); // one big bucket

    std::list<uint32_t> getElements(glm::ivec3 _position);
    void addElements(glm::ivec3 _position, std::list<uint32_t>& _elements);
    uint32_t getIdx(glm::ivec3 _position) const;
    glm::ivec3 denormalize(glm::vec3 _normalizedPosition) const;
    uint32_t getIdx(glm::vec3 _normalizedPosition) const;

    const uint32_t m_maxGridResolution = 100u;
};
}
