#pragma once

#include <QLineEdit>

namespace ux3d::editor {
class CustomLineEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit CustomLineEdit(QWidget* _parent = nullptr);
    explicit CustomLineEdit(const QString& _contents, QWidget* _parent = nullptr);

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* _pEvent) override;

private:
    void contextMenu(QMouseEvent* event);
};
}
