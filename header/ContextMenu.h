#pragma once

#include <list>

class QAction;
class QMenu;
class QPoint;
class QWidget;

namespace ux3d::editor {
class ContextMenu {
public:
    explicit ContextMenu() = default;
    virtual ~ContextMenu() = default;

    void show(QWidget* _parent);
    void addAction(QAction* _action);
    bool isMenuEmpty();

private:
    std::list<QAction*> m_actions;

    void addActionsToMenu(QMenu& _menu);
};
}
