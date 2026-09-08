//
//  QMLDirFileWatcher.cpp
//  Gestaltor
//
//  Created by Phillip Hohenester on 17.12.21.
//

#include "QMLDirFileWatcher.h"

#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QUrl>

using namespace ux3d::editor;

QMLDirFileWatcher* QMLDirFileWatcher::getInstance()
{
    static QMLDirFileWatcher instance(nullptr);
    return &instance;
}

QMLDirFileWatcher::QMLDirFileWatcher(QObject* parent)
    : QObject(parent)
{
#ifndef NDEBUG
    QString qmlDir = QString::fromStdString(UX3D_QML_ROOT_DIR_URL);

    this->filesystemWatcher = new QFileSystemWatcher();
    this->filesystemWatcher->addPath(qmlDir);

    QString localURL = QUrl(qmlDir).toLocalFile();

    QDirIterator it(localURL, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        this->filesystemWatcher->addPath(filePath);
    }

    connect(this->filesystemWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        emit filesChanged();
    });
#endif
}
