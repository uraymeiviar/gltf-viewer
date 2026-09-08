
#include "Actions.h"
#include "HotReloadQQuickWidget.h"
#include "model/ProjectManager.h"
#include "widgets/CustomTitleWidget.h"
#include "widgets/MainWindow.h"

#include <QAction>
#include <QQmlProperty>
#include <QQuickItem>

#include "widgets/Hierarchy.h"

using namespace ux3d;
using namespace ux3d::editor;

Hierarchy::Hierarchy(QWidget* _pParent, QQmlEngine* engine, bool isResources)
    : CustomDockWidget(_pParent)
    , m_pQMLEngine(engine)
{
    QString type = isResources ? "resources" : "hierarchy";
    QString typeUpper = isResources ? "Resources" : "Hierarchy";
    setObjectName(type);
    setAccessibleName(type);
    setFocusPolicy(Qt::StrongFocus);

    m_pQMLWidget = new HotReloadQQuickWidget(engine, this);
    m_pQMLWidget->setRootContext(type, this);
    m_pQMLWidget->setSource(QUrl(QString(UX3D_QML_ROOT_DIR_URL) + "/hierarchy.qml"));
    m_pQMLWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_pQMLWidget->setMinimumWidth(300);
    this->setWidget(m_pQMLWidget);

    // this fixes a load issue where the hierarchy is only shown correctly after a reload.
    m_pQMLWidget->reload();

    QQuickItem* item = m_pQMLWidget->rootObject();
    QQmlProperty p(item, "hierarchyOrResource2");
    p.write(typeUpper);
    QMetaObject::invokeMethod(item, "createTree2");
    // QObject::connect(item, SIGNAL(createTwinPanel()), this, SLOT(createNewHierarchyPanel()));
}

Hierarchy::~Hierarchy()
{
}

void Hierarchy::createNewHierarchyPanel()
{
    Hierarchy* pHierarchyDock = new Hierarchy(this->parentWidget(), m_pQMLEngine);
    pHierarchyDock->setTitle("Resources");
    pHierarchyDock->setTitleBar(new CustomTitleWidget(pHierarchyDock->windowTitle()));

    connect(Actions::instance().showHierarchy, &QAction::triggered, pHierarchyDock, [pHierarchyDock](bool visible) { if (visible) pHierarchyDock->show(); else pHierarchyDock->hide(); });
    connect(pHierarchyDock, &CustomDockWidget::visibilityChanged, Actions::instance().showHierarchy, &QAction::setChecked);
    pHierarchyDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    MainWindow::getMainWindow()->addDockWidget(Qt::LeftDockWidgetArea, pHierarchyDock, Qt::Horizontal);
    QQmlProperty p(pHierarchyDock->m_pQMLWidget->rootObject(), "hierarchyOrResource2");
    p.write("Resources");
    QMetaObject::invokeMethod(pHierarchyDock->m_pQMLWidget->rootObject(), "createTree");
}
