#pragma once

#include <QComboBox>
#include <QWheelEvent>

namespace ux3d::editor {
class ExtensionLabel;

class CustomComboBox : public QComboBox {
public:
    explicit CustomComboBox(QWidget* parent = nullptr);

    void addExtensionLabelToItem(int _index, ExtensionLabel* _label);

    void addExtensionLabelToComboBox(int _index);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    ExtensionLabel* m_comBoBoxExtensionLabel = nullptr;
};
}
