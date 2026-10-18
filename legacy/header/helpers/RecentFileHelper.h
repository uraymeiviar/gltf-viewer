#pragma once

#include <QMenu>
#include <QQueue>
#include <QString>

class QSettings;

namespace ux3d::editor::utils {
class RecentFilesHelper {
private:
    QQueue<QString> m_recentFileNames;
    uint8_t m_maxFiles = 10;
    QMenu m_recentMenu;
    void updateMenu();
    void updateFileQueue();

public:
    RecentFilesHelper(QSettings& _settings);

    QMenu& getRecentFilesMenu() { return m_recentMenu; }
    void fetchRecentFilesFromUserConfig(QSettings& _settings);
    bool storeRecentFilesInUserConfig(QSettings& _settings);
    void addToFileQueue(const QString& _filename);
    QString getFilenameByIndex(int _index);
    uint8_t getMaxFileCount();
    uint8_t getCurrentFileCount();
    void moveRecentFileToTop(uint32_t _index);

    void clear();
    void update();
};
}
