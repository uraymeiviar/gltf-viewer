#include "promoters/Slider.h"
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSlider>

using namespace ux3d;
using namespace ux3d::editor;

void Slider::mousePressEvent(QMouseEvent* _pEvent)
{
    if (_pEvent->button() == Qt::LeftButton && orientation() == Qt::Horizontal) {
        QStyleOptionSlider styleOption;
        initStyleOption(&styleOption);
        const QRect sliderHandleRect = style()->subControlRect(QStyle::CC_Slider, &styleOption, QStyle::SC_SliderHandle, this);
        const QPoint eventPosition = _pEvent->pos();
        if (sliderHandleRect.contains(eventPosition) == false) {
            const int sliderPos = minimum() + ((maximum() - minimum()) * eventPosition.x()) / width();
            setValue(sliderPos);
            sliderMoved(sliderPos);
            _pEvent->accept();
        }
    }

    QSlider::mousePressEvent(_pEvent);
}
