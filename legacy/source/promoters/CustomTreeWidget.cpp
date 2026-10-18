#include "promoters/CustomTreeWidget.h"

#include <QMouseEvent>

using namespace ux3d;
using namespace ux3d::editor;

void CustomTreeWidget::hideColumns()
{
    for (int column = 0; column < columnCount(); column++) {
        hideColumn(column);
    }
}

void CustomTreeWidget::showColumns()
{
    for (int column = 0; column < columnCount(); column++) {
        showColumn(column);
    }
}

void CustomTreeWidget::clickedOut()
{
    clearSelection();
    emit selectionCleared();
}

void CustomTreeWidget::mousePressEvent(QMouseEvent* _pEvent)
{
    if (indexAt(_pEvent->pos()).isValid() == false && _pEvent->button() == Qt::LeftButton) {
        clickedOut();
    }
    QTreeWidget::mousePressEvent(_pEvent);
}

void CustomTreeWidget::dropEvent(QDropEvent* _pEvent)
{
    QTreeWidgetItem* pDroppedItem = currentItem();

    const QModelIndex index = indexAt(_pEvent->position().toPoint());
    QTreeWidgetItem* targetItem = itemFromIndex(index);

    emit itemDropReceived(pDroppedItem, targetItem);

    QTreeWidget::dropEvent(_pEvent);
}
