#pragma once

#include <QScrollArea>

namespace ux3d::editor {
class BackPathScrollArea : public QScrollArea {
    Q_OBJECT

public:
    explicit BackPathScrollArea(QWidget* _pParent)
        : QScrollArea(_pParent)
    {
    }

protected:
    void wheelEvent(QWheelEvent* _pEvent) override;
    void resizeEvent(QResizeEvent* _pEvent) override;
};
}
