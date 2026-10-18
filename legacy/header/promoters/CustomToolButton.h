#pragma once

#include <QToolButton>

namespace ux3d::editor {
class CustomToolButton : public QToolButton {
public:
    CustomToolButton(QWidget* _parent = nullptr);
    void setBaseIcon(const QIcon& _icon);
    void setPressedColor(const QColor& _color) { m_pressedColor = _color; }
    void setDisabledColor(const QColor& _color) { m_disabledColor = _color; }
    void setCheckedColor(const QColor& _color) { m_checkedColor = _color; }
    void setUseColorOverlay(bool _useColorOverlay) { m_useColorOverlay = _useColorOverlay; }

protected:
    void changeEvent(QEvent* event) override;
    void actionEvent(QActionEvent* event) override;

private:
    void changeIconColor(const QColor& _color);

    QIcon m_baseIcon;
    QColor m_disabledColor;
    QColor m_pressedColor;
    QColor m_checkedColor;
    bool m_useColorOverlay;
};
}
