#pragma once
#include "gltf/GlTypes.h"

namespace ux3d::gltf {
enum class UsageFlags : uint32_t {
    None = 0,
    MeshPositions = 1 << 0,
    MeshNormals = 1 << 1,
    MeshTangents = 1 << 2,
    MeshTexcoords = 1 << 3,
    MeshIndices = 1 << 4,
    MeshColor = 1 << 5,
    MeshJoints = 1 << 6,
    MeshWeights = 1 << 7,
    MorphPositions = 1 << 8,
    MorphNormals = 1 << 9,
    MorphTangents = 1 << 10,
    AnimationTranslations = 1 << 11,
    AnimationRotations = 1 << 12,
    AnimationScales = 1 << 13,
    AnimationWeights = 1 << 14,
    SkinInverseBindMatrix = 1 << 15,
    AnimationTimes = 1 << 16,
    AnimationPointer = 1 << 17
};
using UsageFlag = ux3d::core::Flag<UsageFlags>;

enum class ComponentTypeFlags : uint32_t {
    None = 0,
    Float = 1 << 0,
    Byte = 1 << 1,
    ByteNormalized = 1 << 2,
    UnsignedByte = 1 << 3,
    UnsignedByteNormalized = 1 << 4,
    Short = 1 << 5,
    ShortNormalized = 1 << 6,
    UnsignedShort = 1 << 7,
    UnsignedShortNormalized = 1 << 8,
    UnsignedInt = 1 << 9,
    All = 0xFFFFFFFF
};
using ComponentTypeFlag = ux3d::core::Flag<ComponentTypeFlags>;

inline core::string componentTypeFlagsToString(ComponentTypeFlags _type)
{
    switch (_type) {
    case ComponentTypeFlags::None:
        return "NONE";
    case ComponentTypeFlags::Byte:
        return "BYTE";
    case ComponentTypeFlags::ByteNormalized:
        return "BYTE NORMALIZED";
    case ComponentTypeFlags::UnsignedByte:
        return "UNSIGNED BYTE";
    case ComponentTypeFlags::UnsignedByteNormalized:
        return "UNSIGNED BYTE NORMALIZED";
    case ComponentTypeFlags::Short:
        return "SHORT";
    case ComponentTypeFlags::ShortNormalized:
        return "SHORT NORMALIZED";
    case ComponentTypeFlags::UnsignedShort:
        return "UNSIGNED SHORT";
    case ComponentTypeFlags::UnsignedShortNormalized:
        return "UNSIGNED SHORT NORMALIZED";
    case ComponentTypeFlags::UnsignedInt:
        return "UNSIGNED INT";
    case ComponentTypeFlags::Float:
        return "FLOAT";
    default:
        return "";
    }
}

inline void componentTypeAndNormalizationFromString(const core::string& _text, ComponentType& _outComponentType, bool& _outIsNormalized)
{
    const core::string upperText = core::toUpper(_text);

    if (upperText == "BYTE") {
        _outComponentType = ComponentType::Byte;
        _outIsNormalized = false;
    } else if (upperText == "BYTE NORMALIZED") {
        _outComponentType = ComponentType::Byte;
        _outIsNormalized = true;
    } else if (upperText == "UNSIGNED BYTE") {
        _outComponentType = ComponentType::UnsignedByte;
        _outIsNormalized = false;
    } else if (upperText == "UNSIGNED BYTE NORMALIZED") {
        _outComponentType = ComponentType::UnsignedByte;
        _outIsNormalized = true;
    } else if (upperText == "SHORT") {
        _outComponentType = ComponentType::Short;
        _outIsNormalized = false;
    } else if (upperText == "SHORT NORMALIZED") {
        _outComponentType = ComponentType::Short;
        _outIsNormalized = true;
    } else if (upperText == "UNSIGNED SHORT") {
        _outComponentType = ComponentType::UnsignedShort;
        _outIsNormalized = false;
    } else if (upperText == "UNSIGNED SHORT NORMALIZED") {
        _outComponentType = ComponentType::UnsignedShort;
        _outIsNormalized = true;
    } else if (upperText == "UNSIGNED INT") {
        _outComponentType = ComponentType::UnsignedInt;
        _outIsNormalized = false;
    } else if (upperText == "FLOAT") {
        _outComponentType = ComponentType::Float;
        _outIsNormalized = false;
    } else {
        _outComponentType = ComponentType::None;
        _outIsNormalized = false;
    }
}

inline ComponentTypeFlag getFlagFromType(ComponentType _componentType, bool _isNormalized)
{
    ComponentTypeFlag result = ComponentTypeFlags::None;

    switch (_componentType) {
    case ComponentType::Byte:
        if (_isNormalized) {
            result = ComponentTypeFlags::ByteNormalized;
        } else {
            result = ComponentTypeFlags::Byte;
        }
        break;
    case ComponentType::UnsignedByte:
        if (_isNormalized) {
            result = ComponentTypeFlags::UnsignedByteNormalized;
        } else {
            result = ComponentTypeFlags::UnsignedByte;
        }
        break;
    case ComponentType::Short:
        if (_isNormalized) {
            result = ComponentTypeFlags::ShortNormalized;
        } else {
            result = ComponentTypeFlags::Short;
        }
        break;
    case ComponentType::UnsignedShort:
        if (_isNormalized) {
            result = ComponentTypeFlags::UnsignedShortNormalized;
        } else {
            result = ComponentTypeFlags::UnsignedShort;
        }
        break;
    case ComponentType::UnsignedInt:
        result = ComponentTypeFlags::UnsignedInt;
        break;
    case ComponentType::Float:
        result = ComponentTypeFlags::Float;
        break;
    default:
        break;
    }

    return result;
}

inline ComponentTypeFlag getAllowedFlagsBySize(double _minimum, double _maximum)
{
    ComponentTypeFlag result = ComponentTypeFlags::None;

    if (_minimum >= GlTypes::minimumValueFromComponentType(ComponentType::Byte)
        && _maximum <= GlTypes::maximumValueFromComponentType(ComponentType::Byte)) {
        result |= ComponentTypeFlags::Byte;
    }
    if (_minimum >= GlTypes::minimumValueFromComponentType(ComponentType::UnsignedByte)
        && _maximum <= GlTypes::maximumValueFromComponentType(ComponentType::UnsignedByte)) {
        result |= ComponentTypeFlags::UnsignedByte;
    }
    if (_minimum >= GlTypes::minimumValueFromComponentType(ComponentType::Short)
        && _maximum <= GlTypes::maximumValueFromComponentType(ComponentType::Short)) {
        result |= ComponentTypeFlags::Short;
    }
    if (_minimum >= GlTypes::minimumValueFromComponentType(ComponentType::UnsignedShort)
        && _maximum <= GlTypes::maximumValueFromComponentType(ComponentType::UnsignedShort)) {
        result |= ComponentTypeFlags::UnsignedShort;
    }
    if (_minimum >= GlTypes::minimumValueFromComponentType(ComponentType::UnsignedInt)
        && _maximum <= GlTypes::maximumValueFromComponentType(ComponentType::UnsignedInt)) {
        result |= ComponentTypeFlags::UnsignedInt;
    }
    if (_minimum >= GlTypes::minimumValueFromComponentType(ComponentType::Float)
        && _maximum <= GlTypes::maximumValueFromComponentType(ComponentType::Float)) {
        result |= ComponentTypeFlags::Float;
    }

    result |= ComponentTypeFlags::ByteNormalized;
    result |= ComponentTypeFlags::UnsignedByteNormalized;
    result |= ComponentTypeFlags::ShortNormalized;
    result |= ComponentTypeFlags::UnsignedShortNormalized;

    return result;
}

inline ComponentTypeFlag getAllowedTypesVanilla(UsageFlag _usage)
{
    ComponentTypeFlag result = ComponentTypeFlags::All;
    if (_usage.isSet(UsageFlags::MeshPositions)) {
        const ComponentTypeFlag meshPositions = ComponentTypeFlags::Float;
        result &= meshPositions;
    }
    if (_usage.isSet(UsageFlags::MeshNormals)) {
        const ComponentTypeFlag meshNormals = ComponentTypeFlags::Float;
        result &= meshNormals;
    }
    if (_usage.isSet(UsageFlags::MeshTangents)) {
        const ComponentTypeFlag meshTangents = ComponentTypeFlags::Float;
        result &= meshTangents;
    }
    if (_usage.isSet(UsageFlags::MeshTexcoords)) {
        ComponentTypeFlag meshTexcoords = ComponentTypeFlags::Float;
        meshTexcoords |= ComponentTypeFlags::UnsignedByteNormalized;
        meshTexcoords |= ComponentTypeFlags::UnsignedShortNormalized;
        result &= meshTexcoords;
    }
    if (_usage.isSet(UsageFlags::MeshIndices)) {
        ComponentTypeFlag meshIndices = ComponentTypeFlags::UnsignedByte;
        meshIndices |= ComponentTypeFlags::UnsignedShort;
        meshIndices |= ComponentTypeFlags::UnsignedInt;
        result &= meshIndices;
    }
    if (_usage.isSet(UsageFlags::MeshColor)) {
        ComponentTypeFlag meshColor = ComponentTypeFlags::Float;
        meshColor |= ComponentTypeFlags::UnsignedByteNormalized;
        meshColor |= ComponentTypeFlags::UnsignedShortNormalized;
        result &= meshColor;
    }
    if (_usage.isSet(UsageFlags::MeshJoints)) {
        ComponentTypeFlag meshJoints = ComponentTypeFlags::UnsignedByte;
        meshJoints |= ComponentTypeFlags::UnsignedShort;
        result &= meshJoints;
    }
    if (_usage.isSet(UsageFlags::MeshWeights)) {
        ComponentTypeFlag meshWeights = ComponentTypeFlags::Float;
        meshWeights |= ComponentTypeFlags::UnsignedByteNormalized;
        meshWeights |= ComponentTypeFlags::UnsignedShortNormalized;
        result &= meshWeights;
    }
    if (_usage.isSet(UsageFlags::MorphPositions)) {
        const ComponentTypeFlag morphPositions = ComponentTypeFlags::Float;
        result &= morphPositions;
    }
    if (_usage.isSet(UsageFlags::MorphNormals)) {
        const ComponentTypeFlag morphNormals = ComponentTypeFlags::Float;
        result &= morphNormals;
    }
    if (_usage.isSet(UsageFlags::MorphTangents)) {
        const ComponentTypeFlag morphTangents = ComponentTypeFlags::Float;
        result &= morphTangents;
    }
    if (_usage.isSet(UsageFlags::AnimationTimes)) {
        const ComponentTypeFlag animationTimes = ComponentTypeFlags::Float;
        result &= animationTimes;
    }
    if (_usage.isSet(UsageFlags::AnimationTranslations)) {
        const ComponentTypeFlag animationTranslations = ComponentTypeFlags::Float;
        result &= animationTranslations;
    }
    if (_usage.isSet(UsageFlags::AnimationRotations)) {
        ComponentTypeFlag animationRotations = ComponentTypeFlags::Float;

        animationRotations |= ComponentTypeFlags::ByteNormalized;
        animationRotations |= ComponentTypeFlags::ShortNormalized;

        result &= animationRotations;
    }
    if (_usage.isSet(UsageFlags::AnimationScales)) {
        const ComponentTypeFlag animationScales = ComponentTypeFlags::Float;
        result &= animationScales;
    }
    if (_usage.isSet(UsageFlags::AnimationWeights)) {
        ComponentTypeFlag animationWeights = ComponentTypeFlags::Float;

        result &= animationWeights;
    }
    if (_usage.isSet(UsageFlags::SkinInverseBindMatrix)) {
        const ComponentTypeFlag skinInverseBindMatrix = ComponentTypeFlags::Float;
        result &= skinInverseBindMatrix;
    }
    if (_usage.isSet(UsageFlags::AnimationPointer)) {
        const ComponentTypeFlag animationPointer = ComponentTypeFlags::Float;
        result &= animationPointer;
    }

    return result;
}

inline ComponentTypeFlag getAllowedTypesWithMeshQuantization(UsageFlag _usage)
{
    ComponentTypeFlag result = ComponentTypeFlags::All;
    if (_usage.isSet(UsageFlags::MeshPositions)) {
        ComponentTypeFlag meshPositions = ComponentTypeFlags::Float;
        meshPositions |= ComponentTypeFlags::ByteNormalized;
        meshPositions |= ComponentTypeFlags::UnsignedByteNormalized;
        meshPositions |= ComponentTypeFlags::UnsignedShortNormalized;
        meshPositions |= ComponentTypeFlags::Byte;
        meshPositions |= ComponentTypeFlags::UnsignedByte;
        meshPositions |= ComponentTypeFlags::Short;
        meshPositions |= ComponentTypeFlags::ShortNormalized;
        meshPositions |= ComponentTypeFlags::UnsignedShort;
        result &= meshPositions;
    }
    if (_usage.isSet(UsageFlags::MeshNormals)) {
        ComponentTypeFlag meshNormals = ComponentTypeFlags::Float;
        meshNormals |= ComponentTypeFlags::ByteNormalized;
        meshNormals |= ComponentTypeFlags::ShortNormalized;
        result &= meshNormals;
    }
    if (_usage.isSet(UsageFlags::MeshTangents)) {
        ComponentTypeFlag meshTangents = ComponentTypeFlags::Float;
        meshTangents |= ComponentTypeFlags::ByteNormalized;
        meshTangents |= ComponentTypeFlags::ShortNormalized;
        result &= meshTangents;
    }
    if (_usage.isSet(UsageFlags::MeshTexcoords)) {
        ComponentTypeFlag meshTexcoords = ComponentTypeFlags::Float;
        meshTexcoords |= ComponentTypeFlags::ByteNormalized;
        meshTexcoords |= ComponentTypeFlags::UnsignedByteNormalized;
        meshTexcoords |= ComponentTypeFlags::ShortNormalized;
        meshTexcoords |= ComponentTypeFlags::UnsignedShortNormalized;
        meshTexcoords |= ComponentTypeFlags::Byte;
        meshTexcoords |= ComponentTypeFlags::UnsignedByte;
        meshTexcoords |= ComponentTypeFlags::Short;
        meshTexcoords |= ComponentTypeFlags::UnsignedShort;
        result &= meshTexcoords;
    }
    if (_usage.isSet(UsageFlags::MeshIndices)) {
        ComponentTypeFlag meshIndices = ComponentTypeFlags::UnsignedByte;
        meshIndices |= ComponentTypeFlags::UnsignedShort;
        meshIndices |= ComponentTypeFlags::UnsignedInt;
        result &= meshIndices;
    }
    if (_usage.isSet(UsageFlags::MeshColor)) {
        ComponentTypeFlag meshColor = ComponentTypeFlags::Float;
        meshColor |= ComponentTypeFlags::UnsignedByteNormalized;
        meshColor |= ComponentTypeFlags::UnsignedShortNormalized;
        result &= meshColor;
    }
    if (_usage.isSet(UsageFlags::MeshJoints)) {
        ComponentTypeFlag meshJoints = ComponentTypeFlags::UnsignedByte;
        meshJoints |= ComponentTypeFlags::UnsignedShort;
        result &= meshJoints;
    }
    if (_usage.isSet(UsageFlags::MeshWeights)) {
        ComponentTypeFlag meshWeights = ComponentTypeFlags::Float;
        meshWeights |= ComponentTypeFlags::UnsignedByteNormalized;
        meshWeights |= ComponentTypeFlags::UnsignedShortNormalized;
        result &= meshWeights;
    }
    if (_usage.isSet(UsageFlags::MorphPositions)) {
        ComponentTypeFlag morphPositions = ComponentTypeFlags::Float;
        morphPositions |= ComponentTypeFlags::ByteNormalized;
        morphPositions |= ComponentTypeFlags::ShortNormalized;
        morphPositions |= ComponentTypeFlags::Byte;
        morphPositions |= ComponentTypeFlags::Short;
        result &= morphPositions;
    }
    if (_usage.isSet(UsageFlags::MorphNormals)) {
        ComponentTypeFlag morphNormals = ComponentTypeFlags::Float;
        morphNormals |= ComponentTypeFlags::ByteNormalized;
        morphNormals |= ComponentTypeFlags::ShortNormalized;
        result &= morphNormals;
    }
    if (_usage.isSet(UsageFlags::MorphTangents)) {
        ComponentTypeFlag morphTangents = ComponentTypeFlags::Float;
        morphTangents |= ComponentTypeFlags::ByteNormalized;
        morphTangents |= ComponentTypeFlags::ShortNormalized;
        result &= morphTangents;
    }
    if (_usage.isSet(UsageFlags::AnimationTimes)) {
        const ComponentTypeFlag animationTimes = ComponentTypeFlags::Float;
        result &= animationTimes;
    }
    if (_usage.isSet(UsageFlags::AnimationTranslations)) {
        const ComponentTypeFlag animationTranslations = ComponentTypeFlags::Float;
        result &= animationTranslations;
    }
    if (_usage.isSet(UsageFlags::AnimationRotations)) {
        ComponentTypeFlag animationRotations = ComponentTypeFlags::Float;

        animationRotations |= ComponentTypeFlags::ByteNormalized;
        animationRotations |= ComponentTypeFlags::ShortNormalized;

        result &= animationRotations;
    }
    if (_usage.isSet(UsageFlags::AnimationScales)) {
        const ComponentTypeFlag animationScales = ComponentTypeFlags::Float;
        result &= animationScales;
    }
    if (_usage.isSet(UsageFlags::AnimationWeights)) {
        ComponentTypeFlag animationWeights = ComponentTypeFlags::Float;

        result &= animationWeights;
    }
    if (_usage.isSet(UsageFlags::SkinInverseBindMatrix)) {
        const ComponentTypeFlag skinInverseBindMatrix = ComponentTypeFlags::Float;
        result &= skinInverseBindMatrix;
    }
    if (_usage.isSet(UsageFlags::AnimationPointer)) {
        const ComponentTypeFlag animationPointer = ComponentTypeFlags::Float;
        result &= animationPointer;
    }

    return result;
}

inline ComponentTypeFlag getAllowedTypesVanilla(UsageFlag _usage, double _minimum, double _maximum)
{
    ComponentTypeFlag allowedTypes = getAllowedTypesVanilla(_usage);
    ComponentTypeFlag sizeType = getAllowedFlagsBySize(_minimum, _maximum);
    return allowedTypes &= sizeType;
}

inline ComponentTypeFlag getAllowedTypesWithMeshQuantization(UsageFlag _usage, double _minimum, double _maximum)
{
    ComponentTypeFlag allowedTypes = getAllowedTypesWithMeshQuantization(_usage);
    ComponentTypeFlag sizeType = getAllowedFlagsBySize(_minimum, _maximum);
    return allowedTypes &= sizeType;
}
}
