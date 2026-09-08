#pragma once

#include <QListWidget>

class QMouseEvent;

namespace ux3d::editor {

class CustomListWidget : public QListWidget {
    Q_OBJECT

private:
    void clickedOut();

public:
    CustomListWidget(QWidget* _pParent = nullptr);

    // provide a better estimate for size of the list contents
    virtual QSize sizeHint() const override;

protected:
    void mousePressEvent(QMouseEvent* _pEvent) override;

signals:
    void selectionCleared();
};
}
