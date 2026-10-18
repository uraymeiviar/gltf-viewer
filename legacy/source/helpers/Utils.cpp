#include "helpers/Utils.h"
#include "core/Logger.h"
#include "math/VectorFunctions.h"
#include "math/VectorTypes.h"
#include <QColorDialog>
#include <QObject>
#include <QPainter>
#include <QScreen>
#include <QWidget>

using namespace ux3d;
using namespace ux3d::editor;

QColor utils::pickColor(const std::function<void(const QColor&)>& _liveColorFunction, const QColor& _initialColor, const QString& _widgetTitle, const bool _showTransparency)
{
    QColorDialog colorDialog(nullptr);

    QColorDialog::ColorDialogOptions colorDialogOptions;
    if (_showTransparency) {
        colorDialogOptions |= QColorDialog::ShowAlphaChannel;
    }
    colorDialogOptions |= QColorDialog::DontUseNativeDialog;

    colorDialog.setOptions(colorDialogOptions);
    colorDialog.setWindowTitle(_widgetTitle);
    colorDialog.setWindowFlag(Qt::WindowStaysOnTopHint);

    colorDialog.blockSignals(true);
    colorDialog.setCurrentColor(_initialColor);
    colorDialog.blockSignals(false);

    QObject::connect(&colorDialog, &QColorDialog::currentColorChanged, _liveColorFunction);

    int result = colorDialog.exec();

    if (result) {
        return colorDialog.selectedColor();
    }

    return _initialColor;
}

QString utils::getCssStyleFromColor(const QColor& _color)
{
    std::string backgroundStyle = "background: rgb(" + std::to_string(_color.red()) + ", " + std::to_string(_color.green()) + ", " + std::to_string(_color.blue()) + ");";

    backgroundStyle += "border-bottom: 2px solid qlineargradient(x1:0, y1:0, x2:1, y2:0, stop : " + std::to_string(_color.alphaF() - 0.001) + " white, stop : " + std::to_string(_color.alphaF() + 0.001) + " black);";
    backgroundStyle += "selection-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop : " + std::to_string(_color.alphaF() - 0.001) + " white, stop : " + std::to_string(_color.alphaF() + 0.001) + " black)";

    return QString::fromStdString(backgroundStyle);
}

QString utils::convertBytesToString(size_t _size)
{
    double number = _size;
    uint32_t i = 0;
    for (; i < 4; ++i) {
        if (number / 1000.0 > 1) {
            number /= 1000.0;
        } else {
            break;
        }
    }
    switch (i) {
    case 0:
        return QString::number(_size) + " Bytes";
    case 1:
        return QString::number(number, 'f', 2) + " KB";
    case 2:
        return QString::number(number, 'f', 2) + " MB";
    default:
        return QString::number(number, 'f', 2) + " GB";
    }
}

QString utils::convertSecondsToString(uint32_t inputSeconds)
{
    uint32_t seconds = inputSeconds % 60;
    uint32_t minutes = (inputSeconds / 60) % 60;
    uint32_t hours = inputSeconds / 3600;

    QString result;

    if (hours > 0) {
        result += QString::number(hours) + "h ";
    }

    if (minutes > 0 || hours > 0) {
        result += QString::number(minutes) + "m ";
    }

    result += QString::number(seconds) + "s";

    return result;
}

QColor utils::getColorFromVector(const math::dvec3& _colorVector)
{
    QColor color;
    color.setRgbF(_colorVector.r, _colorVector.g, _colorVector.b);
    return color;
}

QColor utils::getColorFromVector(const math::dvec4& _colorVector)
{
    QColor color;
    color.setRgbF(_colorVector.r, _colorVector.g, _colorVector.b, _colorVector.a);
    return color;
}

math::dvec4 utils::getVec4FromColor(const QColor& _color)
{
    return { _color.redF(),
        _color.greenF(),
        _color.blueF(),
        _color.alphaF() };
}

math::dvec4 utils::getVec4FromColor(const QColor& _color, float alpha)
{
    return { _color.redF(),
        _color.greenF(),
        _color.blueF(),
        alpha };
}

math::dvec3 utils::getVec3FromColor(const QColor& _color)
{
    return { _color.redF(),
        _color.greenF(),
        _color.blueF() };
}

math::dvec4 utils::toLinear(const math::dvec4& _colorVector)
{
    return math::dvec4(toLinear(math::dvec3(_colorVector)), _colorVector.a);
}

constexpr double srgbGamma = 2.2;

math::dvec3 utils::toLinear(const math::dvec3& _colorVector)
{
    return math::convertSRGBToLinear(_colorVector);
}

QColor utils::toLinear(const QColor& _color)
{
    auto linearColor = math::convertSRGBToLinear(getVec4FromColor(_color));
    return getColorFromVector(linearColor);
}

math::dvec4 utils::toSrgb(const math::dvec4& _colorVector)
{
    return math::dvec4(toSrgb(math::dvec3(_colorVector)), _colorVector.a);
}

math::dvec3 utils::toSrgb(const math::dvec3& _colorVector)
{
    return math::convertLinearToSRGB(_colorVector);
}

QColor utils::toSrgb(const QColor& _color)
{
    auto sRgbColor = math::convertLinearToSRGB(getVec4FromColor(_color));
    return getColorFromVector(sRgbColor);
}

QString utils::propertyTypeToMimeType(GltfPropertyType _propertyType)
{
    switch (_propertyType) {
    case GltfPropertyType::Scenes:
        return "Scene";
    case GltfPropertyType::Nodes:
        return "Node";
    case GltfPropertyType::Cameras:
        return "Camera";
    case GltfPropertyType::Lights:
        return "Light";
    case GltfPropertyType::Meshes:
        return "Mesh";
    case GltfPropertyType::Materials:
        return "Material";
    case GltfPropertyType::Textures:
        return "Texture";
    case GltfPropertyType::Images:
        return "Image";
    case GltfPropertyType::Samplers:
        return "Sampler";
    case GltfPropertyType::Skins:
        return "Skin";
    default:
        core::logError("GltfPropertyType to MimeType conversion called with invalid GltfPropertyType");
        return "";
    }
}

GltfPropertyType utils::mimeTypeToPropertyType(const QString& _mimeType)
{
    if (_mimeType == "Scene") {
        return GltfPropertyType::Scenes;
    }
    if (_mimeType == "Node") {
        return GltfPropertyType::Nodes;
    }
    if (_mimeType == "Camera") {
        return GltfPropertyType::Cameras;
    }
    if (_mimeType == "Light") {
        return GltfPropertyType::Lights;
    }
    if (_mimeType == "Mesh") {
        return GltfPropertyType::Meshes;
    }
    if (_mimeType == "Material") {
        return GltfPropertyType::Materials;
    }
    if (_mimeType == "Texture") {
        return GltfPropertyType::Textures;
    }
    if (_mimeType == "Image") {
        return GltfPropertyType::Images;
    }
    if (_mimeType == "Sampler") {
        return GltfPropertyType::Samplers;
    }
    if (_mimeType == "Skin") {
        return GltfPropertyType::Skins;
    }

    core::logError("MimeType to GltfPropertyType conversion called with invalid MimeType");
    return GltfPropertyType::None;
}

QString utils::applyGlobalsToStyle(const QString& _style, const QString& _globals)
{
    QString globalVariables = _globals;
    QString globalStyle = _style;

    if (globalVariables != "") {
        int lineReturnIndex = -1;
        do {
            // Gather line entry
            QString lineEntry = "";
            lineReturnIndex = globalVariables.indexOf('\n');
            if (lineReturnIndex >= 0) {
                lineEntry = globalVariables.left(lineReturnIndex);

                if (lineReturnIndex < globalVariables.length()) {
                    globalVariables = globalVariables.mid(lineReturnIndex + 1);
                }
            } else {
                lineEntry = globalVariables;
            }

            if (lineEntry.length() > 0 && (lineEntry[0] == '@')) {
                // Gather variable and value
                int spaceIndex = lineEntry.indexOf(' ');
                if (spaceIndex >= 0) {
                    QString variable = lineEntry.left(spaceIndex);
                    QString value = lineEntry.mid(spaceIndex + 1);

                    int commentIndex = value.indexOf('$');
                    if (commentIndex >= 0) {
                        value = value.left(commentIndex);
                    }

                    globalStyle = globalStyle.replace(variable, value);
                }
            }
        } while (lineReturnIndex != -1);
    }

    return globalStyle;
}

void utils::changePixmapColor(QPixmap& _pixmap, const QColor& _color)
{
    if (_pixmap.isNull()) {
        return;
    }
    QPainter painter(&_pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);

    painter.setBrush(_color);
    painter.setPen(_color);

    painter.drawRect(_pixmap.rect());
}

void utils::moveToAvailableSpace(QWidget* _widget)
{
    if (_widget == nullptr) {
        return;
    }
    QRect screenGeometry = _widget->screen()->geometry();
    // Geometry can contain negativ values depending on screen position relative to primary screen

    QPoint upperCorner = _widget->pos();
    if (!_widget->isWindow()) {
        upperCorner = _widget->parentWidget()->mapToGlobal(upperCorner);
    }
    if (upperCorner.y() + _widget->height() > screenGeometry.bottom()) {
        int newY = _widget->pos().y() - _widget->height();
        upperCorner.setY(newY);
    }
    if (upperCorner.x() + _widget->width() > screenGeometry.right()) {
        int newX = _widget->pos().x() - _widget->width();
        upperCorner.setX(newX);
    }
    if (upperCorner.y() < screenGeometry.top()) {
        int newY = _widget->pos().y() + _widget->height();
        upperCorner.setY(newY);
    }
    if (upperCorner.x() < screenGeometry.left()) {
        int newX = _widget->pos().x() + _widget->width();
        upperCorner.setX(newX);
    }

    _widget->move(upperCorner);
}
