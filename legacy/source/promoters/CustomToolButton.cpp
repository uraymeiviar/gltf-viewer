#include "promoters/CustomToolButton.h"
#include "Constants.h"
#include "helpers/Utils.h"
#include <QAction>
#include <QActionEvent>
#include <QEvent>
#include <QPainter>

using namespace ux3d::editor;

CustomToolButton::CustomToolButton(QWidget* _parent)
    : QToolButton(_parent)
    , m_baseIcon()
    , m_checkedColor(constants::customToolButtonDisabledIconColor)
    , m_disabledColor(constants::customToolButtonDisabledIconColor)
    , m_pressedColor(constants::customToolButtonPressedIconColor)
    , m_useColorOverlay(true)
{
    connect(this, &QToolButton::toggled, this, [this](bool _checked) {
        if (!_checked) {
            setIcon(m_baseIcon);
        } else {
            changeIconColor(m_checkedColor);
        }
    });
    connect(this, &QToolButton::pressed, this, [this]() {
        changeIconColor(m_pressedColor);
    });
    connect(this, &QToolButton::released, this, [this]() {
        if (!isChecked()) {
            setIcon(m_baseIcon);
        } else {
            changeIconColor(m_checkedColor);
        }
    });
}

void CustomToolButton::changeEvent(QEvent* _event)
{
    QToolButton::changeEvent(_event);
    if (_event->type() == QEvent::Type::EnabledChange) {
        if (isEnabled()) {
            setIcon(m_baseIcon);
        } else {
            changeIconColor(m_disabledColor);
        }
    }
}

void CustomToolButton::setBaseIcon(const QIcon& _icon)
{
    m_baseIcon = _icon;
    setIcon(_icon);
}

void CustomToolButton::actionEvent(QActionEvent* _event)
{
    QToolButton::actionEvent(_event);
    if (_event->type() == QActionEvent::ActionAdded) {
        m_baseIcon = _event->action()->icon();
        return;
    }
    if (_event->type() == QActionEvent::ActionRemoved) {
        m_baseIcon = QIcon();
        return;
    }
}

void CustomToolButton::changeIconColor(const QColor& _color)
{
    if (!icon().isNull() && m_useColorOverlay) {
        QPixmap pixmap = icon().pixmap(iconSize());
        utils::changePixmapColor(pixmap, _color);
        setIcon(QIcon(pixmap));
    }
}
