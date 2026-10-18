#pragma once

#include <QSlider>

namespace ux3d::editor {
class Slider : public QSlider {
    using QSlider::QSlider;

protected:
    void mousePressEvent(QMouseEvent* _pEvent) final;
};
}
