#pragma once

#include "CustomDockWidget.h"
#include "core/Logger.h"

#include <QQmlEngine>

class QListWidgetItem;

namespace ux3d::editor {

class HotReloadQQuickWidget;

class Logger : public CustomDockWidget, private core::LogCallback {
    Q_OBJECT

public:
    explicit Logger(QWidget* parent, QQmlEngine* qmlEngine);
    void log(core::LogLevel level, const char* message) override;

    Q_INVOKABLE void copyTextToClipboard(QString msg);
    Q_INVOKABLE void selectionChanged();

signals:
    void logToQml(QString _message, QString _level);
    void deselect();

private:
    bool m_Scroll = true;

    HotReloadQQuickWidget* m_pQMLWidget;

    QString logLevelToString(core::LogLevel _level);
    void initializeFilterButtons();
    void initializeContextMenu();
    void openContextMenu(QPoint _pos);
};
}
