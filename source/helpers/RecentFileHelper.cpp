#include "helpers/RecentFileHelper.h"
#include "Actions.h"
#include "Preferences.h"
#include "filesystem/FileSystem.h"
#include "helpers/QtFileSystem.h"
#include <QFile>
#include <QSettings>

using namespace ux3d;
using namespace ux3d::editor;
using namespace ux3d::editor::utils;

RecentFilesHelper::RecentFilesHelper(QSettings& _settings)
{
    bool ok;
    uint8_t maxFiles = _settings.value("recentFileCount", 8).toUInt(&ok);
    if (ok) {
        m_maxFiles = maxFiles;
    } else {
        m_maxFiles = 8u;
    }
    fetchRecentFilesFromUserConfig(_settings);
}

void RecentFilesHelper::updateMenu()
{
    m_recentMenu.clear();
    bool recentFilesPresent = false;
    for (int x = m_recentFileNames.size() - 1; x >= 0; --x) {
        filesystem::File gltfFile = QtFileSystem::instance().toFile(m_recentFileNames[x].toStdString());
        if (gltfFile.isFile()) {
            m_recentMenu.addAction(m_recentFileNames[x]);
            recentFilesPresent = true;
        } else {
            m_recentFileNames.removeAt(x);
        }
    }
    m_recentMenu.addSeparator();
    m_recentMenu.addAction("Clear Menu");

    Actions::instance().openRecent->setEnabled(recentFilesPresent);
}

void RecentFilesHelper::update()
{
    updateMenu();
}

void RecentFilesHelper::clear()
{
    m_recentFileNames.clear();
    updateMenu();
}

void RecentFilesHelper::fetchRecentFilesFromUserConfig(QSettings& _settings)
{
    const QString recentFiles = _settings.value("RecentFiles").toString();

    if (recentFiles.isEmpty()) {
        return;
    } else {
        QStringList fileNames = recentFiles.split(",");
        for (QString& fileName : fileNames) {
            addToFileQueue(fileName);
        }
    }
}

bool RecentFilesHelper::storeRecentFilesInUserConfig(QSettings& _settings)
{
    QString filenames = "";
    for (QString& fileName : m_recentFileNames) {
        filenames += fileName;
        if (fileName != m_recentFileNames.last())
            filenames += ",";
    }
    _settings.setValue("RecentFiles", filenames);
    return true;
}

void RecentFilesHelper::addToFileQueue(const QString& _filename)
{
    if (!m_recentFileNames.contains(_filename)) {
        m_recentFileNames.enqueue(_filename);
    } else {
        // move file to top
        int idx = m_recentFileNames.indexOf(_filename);
        if (idx != -1) {
            m_recentFileNames.move(idx, m_recentFileNames.size() - 1);
        }
    }
    updateFileQueue();
}

void RecentFilesHelper::updateFileQueue()
{
    m_maxFiles = Preferences::getInstance()->getMaxRecentFiles();
    while (m_recentFileNames.size() > m_maxFiles) {
        m_recentFileNames.dequeue();
    }
}

QString RecentFilesHelper::getFilenameByIndex(int _index)
{
    if (m_recentFileNames.size() - _index - 1 < 0) {
        return "";
    }
    return m_recentFileNames[m_recentFileNames.size() - _index - 1];
}

uint8_t RecentFilesHelper::getMaxFileCount()
{
    return m_maxFiles;
}

uint8_t RecentFilesHelper::getCurrentFileCount()
{
    return m_recentFileNames.size();
}

void RecentFilesHelper::moveRecentFileToTop(uint32_t _index)
{
    if (m_recentFileNames.size() - _index - 1 < 0) {
        return;
    }

    QString fileName = getFilenameByIndex(_index);

    m_recentFileNames.removeOne(fileName);
    m_recentFileNames.push_back(fileName);
    update();
}
