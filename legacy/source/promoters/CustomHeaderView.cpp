#include "promoters/CustomHeaderView.h"

using namespace ux3d::editor;

CustomHeaderView::CustomHeaderView(Qt::Orientation orientation, QWidget* parent)
    : QHeaderView(orientation, parent)
{
    connect(this, &QHeaderView::geometriesChanged, this, &CustomHeaderView::widthChanged);
}

void CustomHeaderView::widthChanged()
{
    QSize contentWidth = sectionSizeFromContents(0);

    int currentSectionSize = sectionSize(0);
    int headerLength = length();
    int viewportLength = viewport()->size().width();
    if (currentSectionSize < contentWidth.width()) {
        setSectionResizeMode(0, QHeaderView::ResizeToContents);
    } else {
        if (viewportLength > headerLength) {
            setSectionResizeMode(0, QHeaderView::Stretch);
        }
    }
}
