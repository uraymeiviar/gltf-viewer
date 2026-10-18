// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <string>
#include <unordered_map>

#include <QByteArray>

namespace ux3d::editor {
class KeyValueStore {
    struct Entry {
        int32_t nameLength;
        QByteArray name;
        int32_t dataLength;
        QByteArray data;
    };

public:
    static constexpr uint32_t Magic = 0xFAB1;

    KeyValueStore() = default;
    KeyValueStore(const QByteArray& _keyValues);

    bool parse(const QByteArray& _keyValues, const bool _clearEntries = false);

    template <typename T>
    void set(const std::string& _key, const T& _value);

    void set(const std::string& _key, const char* _data, const int32_t _size = -1);

    void set(const std::string& _key, const std::string& _value) { set(_key, _value.c_str(), static_cast<int32_t>(_value.size())); }

    template <typename T>
    bool get(const std::string& _key, T& _value) const;

    // generate bytearray from [key, value] pairs
    QByteArray serialize() const;

    // indicates that this KVS has entries
    operator bool() const { return m_keyValues.empty() == false; }

    void clear();

private:
    std::unordered_map<std::string, QByteArray> m_keyValues;
};

template <typename T>
inline bool KeyValueStore::get(const std::string& _key, T& _value) const
{
    auto it = m_keyValues.find(_key);
    if (it != m_keyValues.end()) {
        if (it->second.size() == sizeof(T)) {
            _value = *reinterpret_cast<const T*>(it->second.data());
            return true;
        }
    }
    return false;
}

template <>
inline bool ux3d::editor::KeyValueStore::get<std::string>(const std::string& _key, std::string& _value) const
{
    auto it = m_keyValues.find(_key);
    if (it != m_keyValues.end()) {
        _value = it->second.data();
        return true;
    }
    return false;
}

template <typename T>
inline void KeyValueStore::set(const std::string& _key, const T& _value)
{
    set(_key, reinterpret_cast<const char*>(&_value), sizeof(T));
}
}
