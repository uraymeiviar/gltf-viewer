#include "promoters/CustomListWidget.h"
#include <QMouseEvent>

ux3d::editor::CustomListWidget::CustomListWidget(QWidget* _pParent)
    : QListWidget(_pParent)
{
}

void ux3d::editor::CustomListWidget::clickedOut()
{
    clearSelection();
    emit selectionCleared();
}

void ux3d::editor::CustomListWidget::mousePressEvent(QMouseEvent* _pEvent)
{
    if (!indexAt(_pEvent->pos()).isValid() && _pEvent->button() == Qt::LeftButton) {
        clickedOut();
    }
    QListView::mousePressEvent(_pEvent);
}

QSize ux3d::editor::CustomListWidget::sizeHint() const
{
    const int size = count();

    if (size == 0) {
        return QListWidget::sizeHint();
    }

    auto getHint = [](QListWidgetItem* pItem) -> QSize {
        QSize hint = pItem->sizeHint();

        if (hint.isEmpty()) {
            QWidget* pWidget = pItem->listWidget()->itemWidget(pItem);

            if (pWidget != nullptr) {
                // use bigger estimate
                hint = pWidget->sizeHint();
                hint = hint.expandedTo(pWidget->frameSize());
            }
        }

        return hint;
    };

    QSize hint = getHint(item(0));

    if (hint.isEmpty()) {
        return QListWidget::sizeHint();
    }

    // get the maximum x dimension
    for (auto i = 1; i < size; ++i) {
        QSize elem = getHint(item(i));

        if (elem.width() > hint.width()) {
            hint.setWidth(elem.width());
        }
    }

    const int space = spacing() * 2; // account for spacing between elements
    hint.setHeight((hint.height() + space) * size);
    return hint;
}
