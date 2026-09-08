// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "datastorage/DataStorage.h"

#include <limits>
#include <stdexcept>

using namespace ux3d;
using namespace ux3d::datastorage;

void DataStorage::ensureIdAvailability(size_t count)
{
    if (m_currentID > std::numeric_limits<ID>::max() - static_cast<ID>(count)) {
        throw std::overflow_error("DataStorage ID overflow");
    }
}

DataStorage::ID DataStorage::insert(const std::vector<uint8_t>& value)
{
    ensureIdAvailability();
    m_data[++m_currentID] = value;
    return m_currentID;
}

DataStorage::ID DataStorage::insert(std::vector<uint8_t>&& value)
{
    ensureIdAvailability();
    m_data[++m_currentID] = std::move(value);
    return m_currentID;
}

DataStorage::ID DataStorage::insert(const uint8_t* data, size_t size)
{
    ensureIdAvailability();
    m_data[++m_currentID] = std::vector<uint8_t>(data, data + static_cast<std::ptrdiff_t>(size));
    return m_currentID;
}

void DataStorage::erase(ID key)
{
    auto it = m_data.find(key);
    if (it == m_data.end()) {
        // element never existed or has already been deleted
        return;
    }
    m_data.erase(it);
}

const std::vector<uint8_t>& DataStorage::at(ID key) const
{
    return m_data.at(key);
}

bool DataStorage::contains(ID key) const
{
    return m_data.contains(key);
}

void DataStorage::clear()
{
    m_data.clear();
    m_currentID = s_invalidID;
}
