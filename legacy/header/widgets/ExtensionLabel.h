#pragma once

#include <QLabel>

#include "Constants.h"
#include "core/String.h"
#include "enums/ExtensionType.h"

namespace ux3d::editor {
class ExtensionLabel : public QLabel {
    Q_OBJECT
public:
    ExtensionLabel(QWidget* _parent, ExtensionType _type, const core::string& _companyName = "",
        int _heightInPixel = constants::extensionLabelHeightInPixel, bool _inHeader = false)
        : QLabel(_parent)
        , m_type(_type)
        , m_companyName(_companyName)
        , m_heightInPixel(_heightInPixel)
        , m_inHeader(_inHeader)
    {
        setMaximumHeight(_heightInPixel);
        QString disabledStyle = "QLabel:disabled { ";
        QString style = "QLabel  { margin-top: 1px; border-radius: 2px; border-style: solid; border-width: 1px; font-size: "
            + QString::number(3 * _heightInPixel / 4) + "px;";

        switch (_type) {
        case ExtensionType::KHR:
            setText("KHR");
            style = style + "background-color: " + constants::KHRLabelBodyColor
                + "; color: " + constants::KHRLabelTextColor
                + "; border-color: " + constants::KHRLabelOutlineColor
                + ";";
            disabledStyle = disabledStyle + "background-color: " + constants::KHRLabelBodyDisabledColor
                + "; color: " + constants::KHRLabelTextDisabledColor
                + "; border-color: " + constants::KHRLabelOutlineDisabledColor
                + ";";
            break;

        case ExtensionType::EXT:
            setText("EXT");
            style = style + "background-color: " + constants::EXTLabelBodyColor
                + "; color: " + constants::EXTLabelTextColor
                + "; border-color: " + constants::EXTLabelOutlineColor
                + ";";
            disabledStyle = disabledStyle + "background-color: " + constants::EXTLabelBodyDisabledColor
                + "; color: " + constants::EXTLabelTextDisabledColor
                + "; border-color: " + constants::EXTLabelOutlineDisabledColor
                + ";";
            break;

        case ExtensionType::Known: {
            setText(QString::fromStdString(_companyName));
            const char* outline = _inHeader ? "#000000" : constants::KNOWNLabelOutlineColor;
            style = style + "background-color: " + constants::KNOWNLabelBodyColor
                + "; color: " + constants::KNOWNLabelTextColor
                + "; border-color: " + outline
                + ";";
            disabledStyle = disabledStyle + "background-color: " + constants::KNOWNLabelBodyDisabledColor
                + "; color: " + constants::KNOWNLabelTextDisabledColor
                + ";";
            break;
        }

        case ExtensionType::Unknown:
            setText("???");
            style = style + "background-color: " + constants::UNKNOWNLabelBodyColor
                + "; color: " + constants::UNKNOWNLabelTextColor
                + "; border-color: " + constants::UNKNOWNLabelOutlineColor
                + "; font-weight: bold;";
            disabledStyle = disabledStyle + "background-color: " + constants::UNKNOWNLabelBodyDisabledColor
                + "; color: " + constants::UNKNOWNLabelTextDisabledColor
                + "; border-color: " + constants::UNKNOWNLabelOutlineDisabledColor
                + ";";
            break;

        case ExtensionType::None:
        default:
            break;
        }
        style += " } " + disabledStyle + "}";
        setStyleSheet(style);
        setSizePolicy(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Maximum);
    }

    ExtensionLabel* clone(QWidget* _parent = nullptr) { return new ExtensionLabel(_parent, m_type, m_companyName, m_heightInPixel, m_inHeader); }

private:
    ExtensionType m_type;
    const core::string m_companyName;
    int m_heightInPixel;
    bool m_inHeader;
};

}
