#pragma once

#include "QPushButton"

namespace ux3d::editor {
class PushButton_Preview : public QPushButton {
    Q_OBJECT

public:
    PushButton_Preview(QWidget* _Parent = nullptr)
        : QPushButton(_Parent)
    {
    }
};
}
