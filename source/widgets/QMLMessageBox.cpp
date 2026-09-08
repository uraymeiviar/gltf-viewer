#include "widgets/QMLMessageBox.h"

using namespace ux3d::editor;

QMLMessageBox::QMLMessageBox(const QString& title, QQmlEngine* _pEngine)
    : HotReloadQQuickView(_pEngine)
{
    setTitle(title);
    setFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);
    setRootContext("messageBox", this);
    setSource(QUrl(QString(UX3D_QML_ROOT_DIR_URL) + "/messageBox.qml"));

    setMaximumWidth(700);
    setMinimumWidth(700);
    setMaximumHeight(450);
    setMinimumHeight(450);
    setModality(Qt::ApplicationModal);
}
