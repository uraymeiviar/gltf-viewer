#pragma once

#include "ui_TitleWidget.h"
#include <QWidget>

class QPixmap;

namespace ux3d::editor {
class CustomTitleWidget : public QWidget, public Ui::widget_titleWidget {
    Q_OBJECT

public:
    CustomTitleWidget(const QString& _Text, QWidget* _Parent = nullptr, const QPixmap* _Pixmap = nullptr);
};
}
