#pragma once

#include "QPushButton"

namespace ux3d::editor {
class PushButton_Color : public QPushButton {
    Q_OBJECT

public:
    PushButton_Color(QWidget* _Parent = nullptr)
        : QPushButton(_Parent)
    {
    }
};
}
