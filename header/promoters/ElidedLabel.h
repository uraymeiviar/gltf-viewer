#pragma once

#include <QFrame>

namespace ux3d::editor {
class ElidedLabel : public QFrame {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

public:
    explicit ElidedLabel(const QString& text, QWidget* parent = nullptr);
    explicit ElidedLabel(QWidget* parent = nullptr);

    void setText(const QString& text);
    const QString& text() const { return content; }
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    QString content;
};
}
