#pragma once

#include "CustomDockWidget.h"
class QQmlEngine;

namespace ux3d::editor {
class HotReloadQQuickWidget;

class Hierarchy final : public CustomDockWidget {
    Q_OBJECT

public:
    explicit Hierarchy(QWidget* _pParent = nullptr, QQmlEngine* engine = nullptr, bool isResources = false);
    ~Hierarchy() final;

public slots:
    void createNewHierarchyPanel();

private:
    HotReloadQQuickWidget* m_pQMLWidget = nullptr;
    QQmlEngine* m_pQMLEngine = nullptr;
};
}
