#include "promoters/CustomLineEdit.h"
#include <QList>
#include <QMenu>
#include <QMouseEvent>

using namespace ux3d::editor;

CustomLineEdit::CustomLineEdit(QWidget* parent)
    : QLineEdit(parent)
{
}

CustomLineEdit::CustomLineEdit(const QString& contents, QWidget* parent)
    : QLineEdit(contents, parent)
{
}

void CustomLineEdit::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        selectAll();
        event->accept();
        return;
    }

    QLineEdit::mouseDoubleClickEvent(event);
}

void CustomLineEdit::mousePressEvent(QMouseEvent* event)
{

    if (event->button() == Qt::RightButton) {
        contextMenu(event);
    } else {
        deselect();

        setCursorPosition(event->globalPosition().x());
        QLineEdit::mousePressEvent(event);
    }
}

void CustomLineEdit::contextMenu(QMouseEvent* event)
{
    QMenu* menu = createStandardContextMenu();
    QList<QAction*> actions = menu->actions();

    foreach (QAction* action, actions) {
        QString actionText = action->text();
        if (actionText.contains("Undo")) {
            menu->removeAction(action);
        }
        if (actionText.contains("Redo")) {
            menu->removeAction(action);
        }
    }

    menu->exec(event->globalPosition().toPoint());
    delete menu;
}
