#pragma once

#include "reactor/Utils.h"

#include <glm.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using namespace ux3d;

namespace ux3d::operations::tangent {

struct PrimitiveData {
    std::vector<glm::vec3> pPosAccessor; /* vertices */
    reactor::utils::AccessorTypeVariant pIndicesAccessor = std::monostate(); /* indices */
    std::vector<glm::vec3> pNormalAccessor; /* normals */
    std::vector<glm::vec2> pUVAccessor; /* texture coordinates */
    std::vector<glm::vec4> outTangents; /* output tangents */
    uint32_t num_faces; /* number of polygons */
};

nlohmann::json create(const nlohmann::json& params, nlohmann::json& state, datastorage::DataStorage& dataStorage);
}
