// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include <QApplication>
#include <QFileOpenEvent>

#include "model/ProjectManager.h"

namespace ux3d::editor {
class GestaltorApplication : public QApplication {
    Q_OBJECT

public:
    GestaltorApplication(int& argc, char** argv)
        : QApplication(argc, argv)
    {
    }

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::FileOpen
            && model::ProjectManager::getInstance()->isCMDMode() == false) {

            QFileOpenEvent* openEvent = static_cast<QFileOpenEvent*>(event);
            QString fileName = openEvent->file();

            emit openFile(fileName.toStdString());
        }

        return QApplication::event(event);
    }

signals:
    void openFile(std::string filename);
};
}
