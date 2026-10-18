#include "ContextMenu.h"
#include "QMenu"
#include "QWidget"

using namespace ux3d;
using namespace ux3d::editor;

void ContextMenu::show(QWidget* _parent)
{
    QMenu contextMenu(_parent);
    addActionsToMenu(contextMenu);
    contextMenu.exec(QCursor::pos());
}

void ContextMenu::addAction(QAction* _action)
{
    m_actions.push_back(_action);
}

bool ContextMenu::isMenuEmpty()
{
    return m_actions.empty();
}

void ContextMenu::addActionsToMenu(QMenu& _menu)
{
    for (QAction* action : m_actions) {
        _menu.addAction(action);
    }
}
