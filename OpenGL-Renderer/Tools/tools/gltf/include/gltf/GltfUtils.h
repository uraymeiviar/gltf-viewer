#pragma once

#include "gltf/AccessorDeinterleaver.h"
#include "gltf/GlTypes.h"
#include "math/VectorTypes.h"
#include <variant>

namespace ux3d::gltf {
class Accessor;
class Primitive;
class Gltf;
class string;

class GltfUtils {
public:
    static std::variant<std::monostate, uint32_t, glm::vec2, glm::vec3, glm::vec4> getElementAtIndex(const Accessor* _accessor, uint32_t _index, bool _considerPadding = true);
    static Accessor* getAttributeAccessor(const Primitive& _primitive, const std::string& _attributeName);
    // check whether primitive has any attribute with the provided string in its name
    // (e.g. "texcoord" in "texcoord_0" attribute).
    static bool hasAnyAccessorWithStringInName(const Primitive& _primitive, const std::string& _string);
    static GltfResult deinterleaveAllAccessors(Gltf& gltf);
};
}
