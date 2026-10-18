#include "widgets/CustomTitleWidget.h"

using namespace ux3d;
using namespace ux3d::editor;

CustomTitleWidget::CustomTitleWidget(const QString& _Text, QWidget* _Parent, const QPixmap* _Pixmap)
    : QWidget(_Parent)
{
    setupUi(this);
    label_icon->hide();

    label_title->setText(_Text);
    pushButton_maximize->hide();

    if (_Pixmap != nullptr) {
        label_icon->show();
        label_icon->setPixmap(*_Pixmap);
        label_icon->setScaledContents(true);
    }
}
