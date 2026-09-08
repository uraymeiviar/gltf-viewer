#include "widgets/CustomDockWidget.h"
#include "widgets/CustomTitleWidget.h"

ux3d::editor::CustomDockWidget::CustomDockWidget(QWidget* _pParent)
    : QDockWidget(_pParent)
{
    setFocusPolicy(Qt::ClickFocus);

    // Disable floating dock widgets to avoid a crash bug for widgets that contain a Qt Quick (QML) view
    setFeatures(QDockWidget::NoDockWidgetFeatures);

    connect(this, &QDockWidget::topLevelChanged, this, &CustomDockWidget::setTitleButtonVisibility);
}

const QString& ux3d::editor::CustomDockWidget::getTitle() const
{
    return m_Title;
}

void ux3d::editor::CustomDockWidget::setTitle(const QString& _title)
{
    m_Title = _title;

    setWindowTitle(_title.toUpper());
}

ux3d::editor::CustomTitleWidget* ux3d::editor::CustomDockWidget::getTitleBar() const
{
    return m_pTitleBar;
}

void ux3d::editor::CustomDockWidget::setTitleBar(CustomTitleWidget* _pTitleBar)
{
    m_pTitleBar = _pTitleBar;

    QDockWidget::setTitleBarWidget(m_pTitleBar);

    if (_pTitleBar != nullptr) {
        connect(m_pTitleBar->pushButton_close, &QPushButton::clicked, this, &QDockWidget::hide);
        connect(m_pTitleBar->pushButton_maximize, &QPushButton::clicked, this, &CustomDockWidget::toggleMaximize);
    }
}

void ux3d::editor::CustomDockWidget::toggleMaximize()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void ux3d::editor::CustomDockWidget::setTitleButtonVisibility(bool _visible)
{
    if (m_pTitleBar == nullptr) {
        return;
    }

    if (_visible) {
        m_pTitleBar->pushButton_maximize->show();
    } else {
        m_pTitleBar->pushButton_maximize->hide();
    }
}
