#pragma once

#include <QTreeWidget>

class QMouseEvent;

namespace ux3d::editor {
class CustomTreeWidget : public QTreeWidget {
    Q_OBJECT

public:
    CustomTreeWidget(QWidget* _pParent = nullptr)
        : QTreeWidget(_pParent)
    {
    }

    void hideColumns();
    void showColumns();

private:
    void clickedOut();

protected:
    void mousePressEvent(QMouseEvent* _pEvent) override;
    void dropEvent(QDropEvent* _pEvent) override;

signals:
    void selectionCleared();
    void itemDropReceived(QTreeWidgetItem* _droppedItem, QTreeWidgetItem* _targetItem);
};
}
