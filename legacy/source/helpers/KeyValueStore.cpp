#include "helpers/KeyValueStore.h"
#include "core/Logger.h"
#include <QDataStream>
#include <QIODevice>

ux3d::editor::KeyValueStore::KeyValueStore(const QByteArray& _keyValues)
{
    parse(_keyValues);
}

bool ux3d::editor::KeyValueStore::parse(const QByteArray& _keyValues, const bool _clearEntries)
{
    if (_keyValues.isEmpty()) {
        return false;
    }

    QDataStream stream(_keyValues);
    uint32_t magic = 0u;

    stream >> magic;

    if (magic != Magic) {
        core::logError("Unknown KVS identifier %x", magic);
        return false;
    }

    if (_clearEntries) {
        m_keyValues.clear();
    }

    while (!stream.atEnd()) {
        Entry entry {};
        stream >> entry.nameLength;
        entry.name.resize(entry.nameLength);
        stream.readRawData(entry.name.data(), entry.nameLength);
        stream >> entry.dataLength;
        entry.data.resize(entry.dataLength);
        stream.readRawData(entry.data.data(), entry.dataLength);

        m_keyValues.emplace(entry.name, entry.data);
    }

    return true;
}

QByteArray ux3d::editor::KeyValueStore::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);

    stream << Magic;

    for (const auto& [key, value] : m_keyValues) {
        stream << static_cast<int32_t>(key.size());
        stream.writeRawData(key.data(), static_cast<int32_t>(key.size()));
        stream << static_cast<int32_t>(value.size());
        stream.writeRawData(value.data(), static_cast<int32_t>(value.size()));
    }

    return data;
}

void ux3d::editor::KeyValueStore::clear()
{
    m_keyValues.clear();
}

void ux3d::editor::KeyValueStore::set(const std::string& _key, const char* _data, const int32_t _size)
{
    m_keyValues.emplace(_key, QByteArray(_data, _size));
}
