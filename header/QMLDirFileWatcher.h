#pragma once

#include <QObject>

class QFileSystemWatcher;

namespace ux3d::editor {

class QMLDirFileWatcher : public QObject {
    Q_OBJECT

public:
    static QMLDirFileWatcher* getInstance();

signals:
    void filesChanged();

private:
    QMLDirFileWatcher(QObject* parent);

    QFileSystemWatcher* filesystemWatcher;
};

}