#include "TangentGenerator.h"
#include "mikktspace/mikktspace.h"
using namespace ux3d;
using namespace ux3d::operations;

/* Mikktspace's API Interface*/
namespace ux3d::operations::tangent {
int get_num_faces(const SMikkTSpaceContext* pContext);

int get_num_verts_of_face(const SMikkTSpaceContext* pContext, const int face_idx);

// get position data of one vertex
void get_position(const SMikkTSpaceContext* pContext,
    float r_co[3],
    const int face_idx,
    const int vert_idx);

// get tex coords data of one vertex
void get_texture_coordinate(const SMikkTSpaceContext* pContext,
    float r_uv[2],
    const int face_idx,
    const int vert_idx);

// get normal data of one vertex
void get_normal(const SMikkTSpaceContext* pContext,
    float r_no[3],
    const int face_idx,
    const int vert_idx);

void set_tspace(const SMikkTSpaceContext* pContext,
    const float fv_tangent[3],
    const float face_sign,
    const int face_idx,
    const int vert_idx);
}

uint32_t indexLookup(reactor::utils::AccessorTypeVariant indicesData, uint32_t idx)
{
    if (std::holds_alternative<std::vector<uint32_t>>(indicesData)) {
        return std::get<std::vector<uint32_t>>(indicesData)[idx];
    }
    if (std::holds_alternative<std::vector<uint16_t>>(indicesData)) {
        return std::get<std::vector<uint16_t>>(indicesData)[idx];
    }
    if (std::holds_alternative<std::vector<uint8_t>>(indicesData)) {
        return std::get<std::vector<uint8_t>>(indicesData)[idx];
    }
    return idx;
}

/* Mikktspace's API Functions*/
int tangent::get_num_faces(const SMikkTSpaceContext* pContext)
{
    PrimitiveData* pPrimData = static_cast<PrimitiveData*>(pContext->m_pUserData);
    return static_cast<int>(pPrimData->num_faces);
}

int tangent::get_num_verts_of_face(const SMikkTSpaceContext* pContext, const int face_idx)
{
    return 3;
}

// get position data of one vertex
void tangent::get_position(const SMikkTSpaceContext* pContext,
    float r_co[3],
    const int face_idx,
    const int vert_idx)
{

    auto primData = static_cast<PrimitiveData*>(pContext->m_pUserData);
    auto idx = static_cast<uint32_t>(face_idx * 3 + vert_idx);
    idx = indexLookup(primData->pIndicesAccessor, idx);
    glm::vec3 position = primData->pPosAccessor[idx];

    r_co[0] = position.x;
    r_co[1] = position.y;
    r_co[2] = position.z;
}

// get tex coords data of one vertex
void tangent::get_texture_coordinate(const SMikkTSpaceContext* pContext,
    float r_uv[2],
    const int face_idx,
    const int vert_idx)
{
    auto primData = static_cast<PrimitiveData*>(pContext->m_pUserData);
    auto idx = static_cast<uint32_t>(face_idx * 3 + vert_idx);
    idx = indexLookup(primData->pIndicesAccessor, idx);
    glm::vec2 uv = primData->pUVAccessor[idx];

    r_uv[0] = uv.x;
    r_uv[1] = uv.y;
}

// get normal data of one vertex
void tangent::get_normal(const SMikkTSpaceContext* pContext,
    float r_no[3],
    const int face_idx,
    const int vert_idx)
{
    auto primData = static_cast<PrimitiveData*>(pContext->m_pUserData);
    auto idx = static_cast<uint32_t>(face_idx * 3 + vert_idx);
    idx = indexLookup(primData->pIndicesAccessor, idx);
    glm::vec3 normal = primData->pNormalAccessor[idx];

    r_no[0] = normal.x;
    r_no[1] = normal.y;
    r_no[2] = normal.z;
}

void tangent::set_tspace(const SMikkTSpaceContext* pContext,
    const float fv_tangent[3],
    const float face_sign,
    const int face_idx,
    const int vert_idx)
{
    auto primData = static_cast<PrimitiveData*>(pContext->m_pUserData);
    std::vector<glm::vec4>& tangentData = primData->outTangents;
    auto idx = static_cast<uint32_t>(face_idx * 3 + vert_idx);
    idx = indexLookup(primData->pIndicesAccessor, idx);

    tangentData[idx].x = fv_tangent[0];
    tangentData[idx].y = fv_tangent[1];
    tangentData[idx].z = fv_tangent[2];
    tangentData[idx].w = -face_sign;
}

nlohmann::json tangent::create(const nlohmann::json& params, nlohmann::json& state, datastorage::DataStorage& dataStorage)
{
    auto path = nlohmann::json::json_pointer(params["path"]); // primitive path

    if (state.contains(path / "attributes" / "TANGENT")) {
        throw std::runtime_error("Tangent data already present");
    }

    auto derefAccessor = [&state](std::string path) -> nlohmann::json::json_pointer {
        auto index = state.at(nlohmann::json::json_pointer(path)).get<uint64_t>();
        return nlohmann::json::json_pointer("/gltf/accessors") / index;
    };

    // throws exception if attribute was not found or unexpected type is given
    PrimitiveData primData;
    primData.pPosAccessor = std::get<std::vector<glm::vec3>>(reactor::utils::getAccessorData(derefAccessor(path / "attributes" / "POSITION"), state, dataStorage));
    primData.pNormalAccessor = std::get<std::vector<glm::vec3>>(reactor::utils::getAccessorData(derefAccessor(path / "attributes" / "NORMAL"), state, dataStorage));
    primData.pUVAccessor = std::get<std::vector<glm::vec2>>(reactor::utils::getAccessorData(derefAccessor(path / "attributes" / "TEXCOORD_0"), state, dataStorage));

    if (state.contains(path / "indices")) {
        primData.pIndicesAccessor = reactor::utils::getAccessorData(derefAccessor(path / "indices"), state, dataStorage);
    }

    struct get_size {
        uint32_t operator()(std::vector<uint8_t> v) { return v.size(); };
        uint32_t operator()(std::vector<uint16_t> v) { return v.size(); };
        uint32_t operator()(std::vector<uint32_t> v) { return v.size(); };

        uint32_t operator()(std::vector<float> v) { return v.size(); };
        uint32_t operator()(std::vector<glm::vec2> v) { return v.size(); };
        uint32_t operator()(std::vector<glm::vec3> v) { return v.size(); };
        uint32_t operator()(std::vector<glm::vec4> v) { return v.size(); };

        uint32_t operator()(std::monostate) { return 0; };
    };

    uint32_t numVerts = primData.pPosAccessor.size();
    if (primData.pIndicesAccessor.index() != 0) {
        numVerts = std::visit(get_size(), primData.pIndicesAccessor);
    }

    if (numVerts == 0) {
        throw std::runtime_error { "Primitive vertex count is zero" };
    }

    primData.num_faces = numVerts / 3;
    primData.outTangents.resize(primData.pPosAccessor.size());

    SMikkTSpaceContext s_context {};
    SMikkTSpaceInterface s_interface {};
    s_context.m_pInterface = nullptr;
    s_context.m_pUserData = &primData;
    s_context.m_pInterface = &s_interface;
    s_interface.m_getNumFaces = tangent::get_num_faces;
    s_interface.m_getNumVerticesOfFace = tangent::get_num_verts_of_face;
    s_interface.m_getPosition = tangent::get_position;
    s_interface.m_getNormal = tangent::get_normal;
    s_interface.m_getTexCoord = tangent::get_texture_coordinate;
    s_interface.m_setTSpaceBasic = tangent::set_tspace;

    if (genTangSpaceDefault(&s_context) == false) {
        throw std::runtime_error { "Failed to generate tangents for this mesh" };
    }

    // Set created tangent data:

    for (auto& vec4 : primData.outTangents) {
        if (glm::length(glm::vec3(vec4)) == 0.0f) {
            throw std::runtime_error("Failed to generate tangents for this mesh");
        }
    }

    nlohmann::json::json_pointer accessorPath = reactor::utils::createAccessorFromData(primData.outTangents, state, dataStorage);

    state.at(path / "attributes")["TANGENT"] = std::stoi(accessorPath.back());

    return { { "info", "success" } };
}
