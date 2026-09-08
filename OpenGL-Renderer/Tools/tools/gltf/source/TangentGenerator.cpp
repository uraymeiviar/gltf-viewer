#include "gltf/TangentGenerator.h"
#include "core/MemoryRange.h"
#include "gltf/Accessor.h"
#include "gltf/AccessorCreation.h"
#include "gltf/BufferView.h"
#include "gltf/GltfUtils.h"
#include "gltf/mesh/Primitive.h"

using namespace ux3d;
using namespace ux3d::gltf;

/* Mikktspace's API */
static int gltf::get_num_faces(const SMikkTSpaceContext* pContext)
{
    PrimitiveData* pPrimData = static_cast<PrimitiveData*>(pContext->m_pUserData);
    return static_cast<int>(pPrimData->num_polys);
}

static int gltf::get_num_verts_of_face(const SMikkTSpaceContext* pContext, const int face_idx)
{
    return 3;
}

// get position data of one vertex
static void gltf::get_position(const SMikkTSpaceContext* pContext,
    float r_co[3],
    const int face_idx,
    const int vert_idx)
{
    auto primData = static_cast<PrimitiveData*>(pContext->m_pUserData);
    auto idx = static_cast<uint32_t>(face_idx * 3 + vert_idx);
    glm::vec3 position;
    if (primData->pIndicesAccessor != nullptr) {
        // get correct data based on indices
        uint32_t vertIndex = std::get<uint32_t>(GltfUtils::getElementAtIndex(primData->pIndicesAccessor, idx, false));
        position = std::get<glm::vec3>(GltfUtils::getElementAtIndex(primData->pPosAccessor, vertIndex));
    } else {
        position = std::get<glm::vec3>(GltfUtils::getElementAtIndex(primData->pPosAccessor, idx));
    }
    r_co[0] = position.x;
    r_co[1] = position.y;
    r_co[2] = position.z;
}

// get tex coords data of one vertex
static void gltf::get_texture_coordinate(const SMikkTSpaceContext* pContext,
    float r_uv[2],
    const int face_idx,
    const int vert_idx)
{
    auto primData = static_cast<PrimitiveData*>(pContext->m_pUserData);
    auto idx = static_cast<uint32_t>(face_idx * 3 + vert_idx);
    glm::vec2 uv;
    if (primData->pIndicesAccessor != nullptr) {
        // get correct data based on indices
        uint32_t vertIndex = std::get<uint32_t>(GltfUtils::getElementAtIndex(primData->pIndicesAccessor, idx, false));
        uv = std::get<glm::vec2>(GltfUtils::getElementAtIndex(primData->pUVAccessor, vertIndex));
    } else {
        uv = std::get<glm::vec2>(GltfUtils::getElementAtIndex(primData->pUVAccessor, idx));
    }
    r_uv[0] = uv.x;
    r_uv[1] = uv.y;
}

// get normal data of one vertex
static void gltf::get_normal(const SMikkTSpaceContext* pContext,
    float r_no[3],
    const int face_idx,
    const int vert_idx)
{
    auto primData = static_cast<PrimitiveData*>(pContext->m_pUserData);
    auto idx = static_cast<uint32_t>(face_idx * 3 + vert_idx);
    glm::vec3 normal;
    if (primData->pIndicesAccessor != nullptr) {
        // get correct data based on indices
        uint32_t vertIndex = std::get<uint32_t>(GltfUtils::getElementAtIndex(primData->pIndicesAccessor, idx, false));
        normal = std::get<glm::vec3>(GltfUtils::getElementAtIndex(primData->pNormalAccessor, vertIndex));
    } else {
        normal = std::get<glm::vec3>(GltfUtils::getElementAtIndex(primData->pNormalAccessor, idx));
    }
    r_no[0] = normal.x;
    r_no[1] = normal.y;
    r_no[2] = normal.z;
}

static void gltf::set_tspace(const SMikkTSpaceContext* pContext,
    const float fv_tangent[3],
    const float face_sign,
    const int face_idx,
    const int vert_idx)
{
    auto primData = static_cast<PrimitiveData*>(pContext->m_pUserData);
    std::vector<glm::vec4>& tangentData = primData->outTangents;

    auto idx = static_cast<uint32_t>(face_idx * 3 + vert_idx);
    if (primData->pIndicesAccessor != nullptr) {
        // set correct data based on indices
        idx = std::get<uint32_t>(GltfUtils::getElementAtIndex(primData->pIndicesAccessor, idx, false));
    }
    tangentData[idx].x = fv_tangent[0];
    tangentData[idx].y = fv_tangent[1];
    tangentData[idx].z = fv_tangent[2];
    tangentData[idx].w = face_sign;
}

TangentGenerator::TangentGenerator(Gltf& _gltf, Primitive& _primitive)
    : m_gltf(_gltf)
    , m_primitive(_primitive)
{
}

bool TangentGenerator::execute()
{
    // check if tangent data already present
    if (GltfUtils::getAttributeAccessor(m_primitive, "TANGENT") != nullptr) {
        core::logInfo("Tangent generation aborted. Tangent data already present.");
        return false;
    }

    PrimitiveData primData;
    primData.pPosAccessor = GltfUtils::getAttributeAccessor(m_primitive, "POSITION");
    primData.pIndicesAccessor = m_primitive.getIndicesAccessor();
    primData.pNormalAccessor = GltfUtils::getAttributeAccessor(m_primitive, "NORMAL");
    primData.pUVAccessor = GltfUtils::getAttributeAccessor(m_primitive, "TEXCOORD_0");
    if (primData.pPosAccessor == nullptr || primData.pNormalAccessor == nullptr || primData.pUVAccessor == nullptr) {
        core::logWarning("Tangent generation needs normals, texcoord, and vertex attributes.");
        return false;
    }
    uint32_t numVerts = getNumberOfVertices();
    if (numVerts == 0) {
        core::logWarning("Primitive vertex count is zero.");
        return false;
    }
    primData.num_polys = numVerts / 3;
    primData.outTangents.resize(primData.pPosAccessor->getCount());

    SMikkTSpaceContext s_context {};
    SMikkTSpaceInterface s_interface {};
    s_context.m_pInterface = nullptr;
    s_context.m_pUserData = &primData;
    s_context.m_pInterface = &s_interface;
    s_interface.m_getNumFaces = gltf::get_num_faces;
    s_interface.m_getNumVerticesOfFace = gltf::get_num_verts_of_face;
    s_interface.m_getPosition = gltf::get_position;
    s_interface.m_getNormal = gltf::get_normal;
    s_interface.m_getTexCoord = gltf::get_texture_coordinate;
    s_interface.m_setTSpaceBasic = gltf::set_tspace;

    try {
        if (genTangSpaceDefault(&s_context) == false) {
            throw std::runtime_error { "" };
        }
    } catch (...) {
        core::logWarning("Mikktspace failed to generate tangents for this mesh!");
        return false;
    }

    AccessorCreation accessorCreation(m_gltf);
    Accessor* pAttributeAccessor = accessorCreation.create(primData.outTangents);
    Attribute* pAttribute = m_primitive.addAttribute();
    if (pAttribute == nullptr || pAttributeAccessor == nullptr) {
        core::logWarning("Tangent generation failed. Attribute could not be created.");
        return false;
    }

    pAttribute->setName("TANGENT");
    pAttribute->setAccessor(pAttributeAccessor);

    return true;
}

uint32_t TangentGenerator::getNumberOfVertices()
{
    Accessor* pIndices = m_primitive.getIndicesAccessor();
    if (pIndices != nullptr) {
        return pIndices->getCount();
    }

    Accessor* pPosition = GltfUtils::getAttributeAccessor(m_primitive, "POSITION");
    if (pPosition != nullptr) {
        return pPosition->getCount();
    }
    return 0;
}
