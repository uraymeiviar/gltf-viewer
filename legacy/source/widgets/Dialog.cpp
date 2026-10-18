#include "widgets/Dialog.h"
#include <QPushButton>

using ux3d::editor::Dialog;

Dialog::Dialog(QWidget* _pParent)
    : QDialog(_pParent)
{
    Ui::Dialog::setupUi(this);
    connect(buttonBox->button(QDialogButtonBox::Close), &QAbstractButton::clicked, this, &QDialog::done);
}

void Dialog::setText(const QString& _text)
{
    textBrowser->setText(_text);
}
