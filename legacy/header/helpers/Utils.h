#pragma once

#include "enums/GltfPropertyTypes.h"
#include "math/VectorTypes.h"

#include <functional>

#include <QColor>
#include <QString>

class QPixmap;
class QWidget;

namespace ux3d::editor::utils {
QColor pickColor(
    const std::function<void(const QColor&)>& _liveColorFunction,
    const QColor& _initialColor = Qt::white,
    const QString& _widgetTitle = "Color Picker",
    bool _showTransparency = true);

QString getCssStyleFromColor(const QColor& _color);

QColor getColorFromVector(const math::dvec3& _colorVector);
QColor getColorFromVector(const math::dvec4& _colorVector);

QString convertBytesToString(size_t bytes);
QString convertSecondsToString(uint32_t seconds);

math::dvec4 getVec4FromColor(const QColor& _color);
math::dvec4 getVec4FromColor(const QColor& _color, float alpha);
math::dvec3 getVec3FromColor(const QColor& _color);

math::dvec4 toLinear(const math::dvec4& _colorVector);
math::dvec3 toLinear(const math::dvec3& _colorVector);
QColor toLinear(const QColor& _color);

math::dvec4 toSrgb(const math::dvec4& _colorVector);
math::dvec3 toSrgb(const math::dvec3& _colorVector);
QColor toSrgb(const QColor& _color);

QString propertyTypeToMimeType(GltfPropertyType _type);
GltfPropertyType mimeTypeToPropertyType(const QString& _mimeType);

QString applyGlobalsToStyle(const QString& _style, const QString& _globals);

void changePixmapColor(QPixmap& _icon, const QColor& _color);

void moveToAvailableSpace(QWidget* _widget);
}
