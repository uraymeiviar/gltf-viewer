#include "promoters/BackPathScrollArea.h"

#include <QScrollBar>
#include <QWheelEvent>

void ux3d::editor::BackPathScrollArea::wheelEvent(QWheelEvent* _pEvent)
{
    int delta = _pEvent->angleDelta().y();
    int y = horizontalScrollBar()->value();
    int scrollDist = static_cast<int>(-delta / 3);
    horizontalScrollBar()->setValue(y + scrollDist);
}

void ux3d::editor::BackPathScrollArea::resizeEvent(QResizeEvent* _pEvent)
{
    QScrollArea::resizeEvent(_pEvent);
    horizontalScrollBar()->setValue(horizontalScrollBar()->maximum());
}
