#pragma once

#include <QPushButton>

class QAction;

namespace ux3d::editor {
class ActionButton : public QPushButton {
    Q_OBJECT
private:
    QAction* actionOwner = nullptr;

public:
    explicit ActionButton(QWidget* parent = nullptr);
    void setAction(QAction* action);

public slots:
    void updateButtonStatusFromAction();
};
}
