#include "gltf/GltfUtils.h"

#include "gltf/Accessor.h"
#include "gltf/AccessorDeinterleaver.h"
#include "gltf/BufferView.h"
#include "gltf/Gltf.h"
#include "gltf/mesh/Primitive.h"

#include <string>
#include <type_traits>
#include <utility>

using namespace ux3d;
using namespace ux3d::gltf;

std::variant<std::monostate, uint32_t, glm::vec2, glm::vec3, glm::vec4> GltfUtils::getElementAtIndex(const Accessor* _accessor, uint32_t _index, bool _considerPadding)
{
    ComponentType componentType = _accessor->getComponentType();
    AttributeType attributeType = _accessor->getType();
    core::MemoryRange<const uint8_t> memoryRange;
    _accessor->getData(memoryRange);
    const uint8_t* pData = memoryRange.data();

    uint64_t stride = 0;
    if (_accessor->getBufferView() != nullptr) {
        stride = _accessor->getBufferView()->getByteStride().value_or(_accessor->getByteLengthFrom(componentType, attributeType, _considerPadding));
    } else {
        stride = _accessor->getByteLengthFrom(componentType, attributeType, _considerPadding);
    }

    if (attributeType == AttributeType::Scalar) {
        switch (componentType) {
        case ComponentType::UnsignedByte:
            return static_cast<uint32_t>(*reinterpret_cast<const uint8_t*>(pData + (_index * stride)));
        case ComponentType::UnsignedShort:
            return static_cast<uint32_t>(*reinterpret_cast<const uint16_t*>(pData + (_index * stride)));
        case ComponentType::UnsignedInt:
            return static_cast<uint32_t>(*reinterpret_cast<const uint32_t*>(pData + (_index * stride)));
        default:
            break;
        };
    }

    if (componentType != ComponentType::Float) {
        return std::monostate {}; // ToDo: handle other component types as soon as mesh decimation supports them
    }

    switch (attributeType) {
    case AttributeType::Vec2:
        return static_cast<glm::vec2>(*reinterpret_cast<const glm::vec2*>(pData + (_index * stride)));
    case AttributeType::Vec3:
        return static_cast<glm::vec3>(*reinterpret_cast<const glm::vec3*>(pData + (_index * stride)));
    case AttributeType::Vec4:
        return static_cast<glm::vec4>(*reinterpret_cast<const glm::vec4*>(pData + (_index * stride)));
    default:
        break;
    };

    return std::monostate {}; // ToDo: handle other attribute types as soon as mesh decimation supports them
}

Accessor* GltfUtils::getAttributeAccessor(const Primitive& _primitive, const std::string& _attributeName)
{
    for (Attribute& attribute : _primitive.getAttributes()) {
        if (attribute.getName() == _attributeName) {
            return attribute.getAccessor();
        }
    }
    return nullptr;
}

bool GltfUtils::hasAnyAccessorWithStringInName(const Primitive& _primitive, const std::string& _string)
{
    for (Attribute& attribute : _primitive.getAttributes()) {
        if (attribute.getName().find(_string) != std::string::npos) {
            return true;
        }
    }
    return false;
}

GltfResult GltfUtils::deinterleaveAllAccessors(Gltf& gltf)
{
    for (gltf::Accessor& accessor : gltf.getAccessors()) {
        if (accessor.getSparse().has_value()) {
            continue;
        }

        gltf::AccessorDeinterleaver accessorDeinterleaver(gltf, accessor);
        if (AccessorDeinterleaver::isInterleaved(gltf, accessor)) {
            auto result = accessorDeinterleaver.deinterleave();
            if (result != GltfResult::Success) {
                return result;
            }
        }
    }
    return GltfResult::Success;
}
