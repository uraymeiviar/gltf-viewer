#include "gltf/GlTypes.h"
#include <limits>

using namespace ux3d;
using namespace ux3d::gltf;

core::string GlTypes::componentTypeToString(ComponentType _type)
{
    switch (_type) {
    case ComponentType::Byte:
        return "BYTE";
    case ComponentType::UnsignedByte:
        return "UNSIGNED BYTE";
    case ComponentType::Short:
        return "SHORT";
    case ComponentType::UnsignedShort:
        return "UNSIGNED SHORT";
    case ComponentType::UnsignedInt:
        return "UNSIGNED INT";
    case ComponentType::Float:
        return "FLOAT";
    default:
        return "";
    }
}

ComponentType GlTypes::componentTypeFromString(const core::string& _text)
{
    const core::string upperText = core::toUpper(_text);

    if (upperText == "BYTE") {
        return ComponentType::Byte;
    } else if (upperText == "UNSIGNED BYTE") {
        return ComponentType::UnsignedByte;
    } else if (upperText == "SHORT") {
        return ComponentType::Short;
    } else if (upperText == "UNSIGNED SHORT") {
        return ComponentType::UnsignedShort;
    } else if (upperText == "UNSIGNED INT") {
        return ComponentType::UnsignedInt;
    } else if (upperText == "FLOAT") {
        return ComponentType::Float;
    }

    return ComponentType::None;
}

double GlTypes::maximumValueFromComponentType(const ComponentType _type)
{
    switch (_type) {
    case ComponentType::Byte:
        return std::numeric_limits<int8_t>::max();
    case ComponentType::UnsignedByte:
        return std::numeric_limits<uint8_t>::max();
    case ComponentType::Short:
        return std::numeric_limits<int16_t>::max();
    case ComponentType::UnsignedShort:
        return std::numeric_limits<uint16_t>::max();
    case ComponentType::UnsignedInt:
        return std::numeric_limits<uint32_t>::max();
    case ComponentType::Float:
        return std::numeric_limits<float>::max();
    default:
        return 0.0;
    }
}

double GlTypes::minimumValueFromComponentType(const ComponentType _type)
{
    switch (_type) {
    case ComponentType::Byte:
        return std::numeric_limits<int8_t>::lowest();
    case ComponentType::UnsignedByte:
        return std::numeric_limits<uint8_t>::lowest();
    case ComponentType::Short:
        return std::numeric_limits<int16_t>::lowest();
    case ComponentType::UnsignedShort:
        return std::numeric_limits<uint16_t>::lowest();
    case ComponentType::UnsignedInt:
        return std::numeric_limits<uint32_t>::lowest();
    case ComponentType::Float:
        return std::numeric_limits<float>::lowest();
    default:
        return 0.0;
    }
}
