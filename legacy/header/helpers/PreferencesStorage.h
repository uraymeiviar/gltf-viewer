#pragma once
#include "imageformats/imageformats.h"
#include "math/VectorTypes.h"
#include <QString>
#include <QVariant>
#include <map>

namespace ux3d::editor {

// Usage:
//
//-> Register
// Register a variable by providing a pointer to that variable
// and defining an entry name that is subsequently used to store
// said variable in the userConfig.ini file.
//
//-> Load
// By calling load(T& variable) with the address of a registered variable
// the variable will be updated if the respective entry is found
// in the userConfig.ini file. If no entry is found, the variable
// remains unchanged.
//
//-> Save
// To store a registered variable, call save(T& variable).
// If no entry is found in userConfig.ini, a new entry is created.

class PreferencesStorage {

public:
    void registerEntry(void* _variable, const QString& _entryName, const QString& _groupName = "V2");

    template <typename T>
    void load(T& _variable);

    template <typename T>
    void store(T& _variable);

private:
    const QString m_defaultGroupName = "Preferences";

    void storeEntry(const QString& _entryName, const QString& _groupName, math::vec4 _value);
    void storeEntry(const QString& _entryName, const QString& _groupName, QVariant _value);
    QVariant getEntry(const QString& _entryName, const QString& _groupName);
    void getEntry(const QString& _entryName, const QString& _groupName, QVariant& _value);
    math::vec4 getVec4(const QString& _entryName, const QString& _groupName);

    void load(void* _key, QVariant& _value);
    void store(void* _key, QVariant _value);

    std::map<QString, QVariant> m_cache;
    std::map<void*, QString> m_entries;
    std::map<void*, QString> m_groups;

    // helper functions:
    QVariant toQVariant(const math::vec4& _value) const;
    math::vec4 toVec4(const QVariant& _value) const;
};

template <typename T>
inline void PreferencesStorage::load(T& _variable)
{
    QVariant qVariant;
    load((void*)&_variable, qVariant);
    if (!qVariant.isNull() && qVariant.isValid() != false) {
        _variable = qVariant.value<T>();
    }
}

template <>
inline void PreferencesStorage::load<math::vec4>(math::vec4& _variable)
{
    QVariant intermediateValue;
    load((void*)&_variable, intermediateValue);
    if (!intermediateValue.isNull()) {
        _variable = toVec4(intermediateValue);
    }
}

template <>
inline void PreferencesStorage::load<math::vec3>(math::vec3& _variable)
{
    QVariant intermediateValue;
    load((void*)&_variable, intermediateValue);
    if (!intermediateValue.isNull()) {
        _variable = math::vec3(toVec4(intermediateValue));
    }
}

template <>
inline void PreferencesStorage::load<imageformats::ImageEncoding>(imageformats::ImageEncoding& _variable)
{
    QVariant intermediateValue;
    load((void*)&_variable, intermediateValue);
    if (!intermediateValue.isNull()) {
        _variable = static_cast<imageformats::ImageEncoding>(intermediateValue.toInt());
    }
}

template <typename T>
inline void PreferencesStorage::store(T& _variable)
{
    store((void*)&_variable, _variable);
}

template <>
inline void PreferencesStorage::store<math::vec4>(math::vec4& _variable)
{
    store((void*)&_variable, toQVariant(_variable));
}

template <>
inline void PreferencesStorage::store<math::vec3>(math::vec3& _variable)
{
    store((void*)&_variable, toQVariant(math::vec4(_variable, 0.0)));
}

template <>
inline void PreferencesStorage::store<imageformats::ImageEncoding>(imageformats::ImageEncoding& _variable)
{
    store((void*)&_variable, QVariant(static_cast<int>(_variable)));
}
}
