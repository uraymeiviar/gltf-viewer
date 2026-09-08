#include "helpers/PreferencesStorage.h"
#include "core/Logger.h"
#include "filesystem/Dir.h"
#include "filesystem/File.h"
#include "helpers/QtFileSystem.h"
#include <QList>
#include <QSettings>
#include <QStandardPaths>

using namespace ux3d;
using namespace ux3d::editor;

void PreferencesStorage::storeEntry(const QString& _entryName, const QString& _groupName, math::vec4 _value)
{
    storeEntry(_entryName, _groupName, QList<QVariant> { _value.x, _value.y, _value.z, _value.w });
}

void PreferencesStorage::storeEntry(const QString& _entryName, const QString& _groupName, QVariant _value)
{

    if (m_cache.count(_entryName) != 0) {
        m_cache.erase(_entryName);
    }

    QString applicationDataPath = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    QSettings userConfig(applicationDataPath + "/userConfig.ini", QSettings::IniFormat);

    userConfig.beginGroup(_groupName);

    userConfig.setValue(_entryName, _value);

    userConfig.endGroup();

    userConfig.sync();
}

QVariant PreferencesStorage::getEntry(const QString& _entryName, const QString& _groupName)
{
    QVariant value;
    QString applicationDataPath = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    QSettings userConfig(applicationDataPath + "/userConfig.ini", QSettings::IniFormat);

    userConfig.beginGroup(_groupName);

    if (userConfig.contains(_entryName)) {
        value = userConfig.value(_entryName);
    }

    userConfig.endGroup();
    userConfig.sync();

    m_cache[_entryName] = value;
    return value;
}

math::vec4 PreferencesStorage::getVec4(const QString& _entryName, const QString& _groupName)
{
    math::vec4 vecValue(0, 0, 0, 0);

    QVariant entry = getEntry(_entryName, _groupName);
    if (entry.isNull()) {
        return vecValue;
    }

    QList<QVariant> loadedList = entry.toList();

    for (int i = 0; i < loadedList.size() && i < 4; i++) {
        vecValue[i] = loadedList[static_cast<int>(i)].toDouble();
    }

    return vecValue;
}

void PreferencesStorage::getEntry(const QString& _entryName, const QString& _groupName, QVariant& _value)
{
    QVariant value; // is Null
    value = getEntry(_entryName, _groupName); // returns Null Variant if entry does not exist

    if (!value.isNull()) {
        _value = value;
    }
}

void PreferencesStorage::load(void* _key, QVariant& _value)
{
    if (m_entries[_key].count() == 0 || m_groups[_key].count() == 0) {
        //error: unregistered entry encountered
        return;
    }

    QString entryName = m_entries[_key];
    QString groupName = m_groups[_key];
    getEntry(entryName, groupName, _value);
}

void PreferencesStorage::store(void* _key, QVariant _value)
{
    if (m_entries[_key].count() == 0 || m_groups[_key].count() == 0) {
        //error: unregistered entry encountered
        return;
    }

    QString entryName = m_entries[_key];
    QString groupName = m_groups[_key];

    storeEntry(entryName, groupName, _value);
}

void PreferencesStorage::registerEntry(void* _variable, const QString& _entryName, const QString& _groupName)
{
    m_entries[_variable] = _entryName;

    if (_groupName.isEmpty()) {
        m_groups[_variable] = m_defaultGroupName;
    } else {
        m_groups[_variable] = _groupName;
    }
}

QVariant PreferencesStorage::toQVariant(const math::vec4& _value) const
{
    return QList<QVariant> { (double)_value.x, (double)_value.y, (double)_value.z, (double)_value.w };
}

math::vec4 PreferencesStorage::toVec4(const QVariant& _value) const
{
    math::vec4 vecValue = math::vec4(0);
    QList<QVariant> loadedList = _value.toList();

    for (int i = 0; i < loadedList.size() && i < 4; i++) {
        vecValue[i] = loadedList[static_cast<int>(i)].toDouble();
    }

    return vecValue;
}
