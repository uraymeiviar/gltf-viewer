#include "widgets/Logger.h"
#include "HotReloadQQuickWidget.h"
#include "SelectionInfo.h"
#include "SelectionManager.h"
#include <QApplication>
#include <QClipboard>

using namespace ux3d;
using namespace ux3d::editor;

Logger::Logger(QWidget* _parent, QQmlEngine* _qmlEngine)
    : CustomDockWidget(_parent)
{
    setObjectName("logger");

    m_pQMLWidget = new HotReloadQQuickWidget(_qmlEngine, this);
    m_pQMLWidget->setRootContext("logger", this);
    m_pQMLWidget->setSource(QUrl(QString(UX3D_QML_ROOT_DIR_URL) + "/logger.qml"));
    m_pQMLWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_pQMLWidget->setMinimumWidth(300);
    this->setWidget(m_pQMLWidget);

    core::addLoggerCallback(this);
    connect(SelectionManager::getInstance(), &SelectionManager::selectObjects, this, [this](const std::vector<SelectionInfo>& selection) {
        if (selection.size() > 0 && selection[0].type != GltfPropertyType::None) {
            emit deselect();
        }
    });

    // this fixes a load issue where the logger is only shown correctly after a reload.
    m_pQMLWidget->reload();
}

void Logger::log(core::LogLevel _Level, const char* _pMessage)
{
    emit logToQml(QString(_pMessage).trimmed(), logLevelToString(_Level));
}

void Logger::copyTextToClipboard(QString msg)
{
    QClipboard* p_Clipboard = QApplication::clipboard();
    p_Clipboard->setText(msg);
}

void Logger::selectionChanged()
{
    SelectionInfo selectionInfo;
    selectionInfo.type = GltfPropertyType::None;
    selectionInfo.source = SelectionSource::None;
    const std::vector<SelectionInfo> selectionInfoList = { selectionInfo };
    emit SelectionManager::getInstance()->selectObjects(selectionInfoList);
}

QString Logger::logLevelToString(core::LogLevel _level)
{
    switch (_level) {
    case core::LogLevel::Warning: {
        return "warning";
    }
    case core::LogLevel::Error: {
        return "error";
    }
    case core::LogLevel::Fatal: {
        return "fatal";
    }
    case core::LogLevel::Info:
    default: {
        return "info";
    }
    }
}
