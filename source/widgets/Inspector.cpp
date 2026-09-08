#include "widgets/Inspector.h"
#include "HotReloadQQuickWidget.h"

#include <QBoxLayout>

using namespace ux3d;
using namespace ux3d::editor;

Inspector::Inspector(QWidget* _Parent, QQmlEngine* engine)
    : CustomDockWidget(_Parent)
{
    setObjectName("inspector");

    m_qmlWidget = new HotReloadQQuickWidget(engine, this);
    m_qmlWidget->setRootContext("inspector", this);
    m_qmlWidget->setSource(QUrl(QString(UX3D_QML_ROOT_DIR_URL) + "/inspector.qml"));
    m_qmlWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_qmlWidget->setMinimumWidth(400);
    this->setWidget(m_qmlWidget);

    m_qmlWidget->reload();
}
