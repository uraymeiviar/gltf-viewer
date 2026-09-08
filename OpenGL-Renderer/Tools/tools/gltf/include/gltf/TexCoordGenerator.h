#pragma once

#include "math/VectorTypes.h"
#include <limits>
#include <vector>

namespace ux3d::gltf {
class Gltf;
class Primitive;
class Accessor;

enum GenerationType {
    Spherical,
    Cylindrical,
    Planar,
    Cube
};

enum CubeFace {
    Top,
    Bottom,
    Left,
    Right,
    Front,
    Back
};

/**
 * @brief stores a combination of tex coords and corresponding vertex index (for correct indexing)
 *
 */
struct CubeProjHelper {
    uint32_t vertIdx;
    glm::vec2 texCoords;
    CubeProjHelper(uint32_t _vertIdx, glm::vec2 _texCoords)
        : vertIdx(_vertIdx)
        , texCoords(_texCoords)
    {
    }
};

/**
 * @brief stores the tex coords for one cube side. Also stores min and max values (for later scaling).
 *
 */
struct CubeProjBin {
    std::vector<CubeProjHelper> data;
    // these values are used to scale the
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::min();
    float maxY = std::numeric_limits<float>::min();
};

class TexCoordGenerator {
public:
    TexCoordGenerator(gltf::Gltf& _gltf, gltf::Primitive& _primitive);

    bool generate(uint32_t texCoordIdx, GenerationType genType, glm::vec3 normal = glm::vec3(0, 0, 1));

private:
    float inclinationFromCartesian(glm::vec3 normal);
    float azimuthFromCartesian(glm::vec3 normal);
    float inclinationFromSpherical(float inclination, glm::vec3 normal);

    void createTexCoordsSpherical(std::vector<glm::vec2>& generatedTexCoords, uint32_t vertexCount, const Accessor* pPosAccessor, const Accessor* pNormalAccessor);
    void createTexCoordsCylindrical(std::vector<glm::vec2>& generatedTexCoords, uint32_t vertexCount, const Accessor* pPosAccessor, const Accessor* pNormalAccessor);
    void createTexCoordsPlanar(std::vector<glm::vec2>& generatedTexCoords, uint32_t vertexCount, const Accessor* pPosAccessor, glm::vec3 normal);
    void createTexCoordsCube(std::vector<glm::vec2>& generatedTexCoords, uint32_t vertexCount, const Accessor* pPosAccessor, const Accessor* pNormalAccessor);

    /**
     * @brief checks if a tex coord attribute with the specified index can be created.
     * It cannot be created if its index is more than one bigger than the highest already present tex coord.
     *
     * @param idx
     * @return true
     * @return false
     */
    bool canCreateTexCoord(uint32_t idx);
    void removeAlreadyExistingTexCoordAttribute(uint32_t idx);

    /**
     * @brief calculates the planar texcoords for this postion projected onto the specified plane.
     *
     * @param position
     * @param planeVec1 specifies plane
     * @param planeVec2 specifies plane
     * @return glm::vec2
     */
    glm::vec2 calcTexCoordPlanar(const glm::vec3& position, const glm::vec3& normal);

    float rescale(float x, float origMin, float origMax, float targetMin, float targetMax);

    /**
     * @brief Get the cube face the provided normal vector points into
     *
     * @param normal
     * @return CubeFace
     */
    CubeFace getCubeFaceFromVector(const glm::vec3& normal);

    float angleBetweenVectors(const glm::vec3& a, const glm::vec3& b);

    void addTexCoordToCubeProjBin(CubeProjBin& bin, glm::vec2 texCoord, uint32_t vertIdx);
    /**
     * @brief scales all values in this bin to be between zero and one
     */
    void fixScalesInCubeProjBin(CubeProjBin& bin);

    gltf::Gltf& m_gltf;
    gltf::Primitive& m_primitive;
};
}
