// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ux3d::datastorage {
class DataStorage {
public:
    using ID = uint64_t;

    ID insert(const std::vector<uint8_t>& value);
    ID insert(std::vector<uint8_t>&& value);
    ID insert(const uint8_t* data, size_t size);
    void erase(ID key);
    const std::vector<uint8_t>& at(ID key) const;
    bool contains(ID key) const;

    void clear();

private:
    static constexpr ID s_invalidID = 0;

    void ensureIdAvailability(size_t count = 1);

    ID m_currentID = s_invalidID;
    std::unordered_map<ID, std::vector<uint8_t>> m_data;
};
}
