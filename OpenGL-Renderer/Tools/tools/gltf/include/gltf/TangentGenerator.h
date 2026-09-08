#pragma once
#include "mikktspace/mikktspace.h"

#include "math/VectorTypes.h"
#include <string>
#include <variant>
#include <vector>

using namespace ux3d;

namespace ux3d::gltf {
class Gltf;
class Primitive;
class Accessor;

/* User data. */
struct PrimitiveData {
    const Accessor* pPosAccessor; /* vertices */
    const Accessor* pIndicesAccessor; /* indices */
    const Accessor* pNormalAccessor; /* normals */
    const Accessor* pUVAccessor; /* texture coordinates */
    std::vector<glm::vec4> outTangents; /* output tangents */
    uint32_t num_polys; /* number of polygons */
};

/* Mikktspace's API */
static int get_num_faces(const SMikkTSpaceContext* pContext);

static int get_num_verts_of_face(const SMikkTSpaceContext* pContext, const int face_idx);

// get position data of one vertex
static void get_position(const SMikkTSpaceContext* pContext,
    float r_co[3],
    const int face_idx,
    const int vert_idx);

// get tex coords data of one vertex
static void get_texture_coordinate(const SMikkTSpaceContext* pContext,
    float r_uv[2],
    const int face_idx,
    const int vert_idx);

// get normal data of one vertex
static void get_normal(const SMikkTSpaceContext* pContext,
    float r_no[3],
    const int face_idx,
    const int vert_idx);

static void set_tspace(const SMikkTSpaceContext* pContext,
    const float fv_tangent[3],
    const float face_sign,
    const int face_idx,
    const int vert_idx);

class TangentGenerator {
public:
    TangentGenerator(gltf::Gltf& _gltf, gltf::Primitive& _primitive);
    bool execute();

private:
    uint32_t getNumberOfVertices();

    gltf::Gltf& m_gltf;
    gltf::Primitive& m_primitive;
};
}
