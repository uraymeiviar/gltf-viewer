#pragma once

#include <QHeaderView>

namespace ux3d::editor {
class CustomHeaderView : public QHeaderView {
    Q_OBJECT
public:
    explicit CustomHeaderView(Qt::Orientation orientation = Qt::Horizontal, QWidget* parent = nullptr);

protected slots:
    void widthChanged();
};
}
