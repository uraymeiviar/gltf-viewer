#pragma once

#include "ui_Dialog.h"
#include <QDialog>

namespace ux3d::editor {
class Dialog : public Ui::Dialog,
               public QDialog {
public:
    explicit Dialog(QWidget* _pParent = nullptr);
    void setText(const QString& _text);
};
}
