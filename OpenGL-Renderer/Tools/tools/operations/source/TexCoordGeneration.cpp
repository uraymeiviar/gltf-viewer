#include "TexCoordGeneration.h"

#include "HelperFunctions.h"
#include "operations/Library.h"
#include "reactor/Utils.h"

#include <gtc/constants.hpp>
#include <gtx/quaternion.hpp>

using namespace ux3d;
using namespace ux3d::operations;
using namespace ux3d::operations::mesh;

void mesh::generateTexCoords(const nlohmann::json& params, nlohmann::json& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback)
{
    // get all parameters
    // ------------------------- get all parameters -----------
    auto primitivePath = nlohmann::json::json_pointer(params["path"]);
    uint32_t texCoordIdx = params["texCoordIndex"].get<uint32_t>();

    std::string proxyShape = params["proxyShape"].get<std::string>();

    glm::vec3 normal = params.value("normal", glm::vec3(1, 0, 0));

    // ------------------------- generation ------------------

    // check if tex coord can actually be created
    if (TexCoordHelpers::canCreateTexCoord(texCoordIdx, state.at(primitivePath)) == false) {
        throw std::runtime_error("Texture coordinate generation would create an invalid gltf caused by a gap in the texture coordinate indices.");
    }

    glm::vec3 zeros(0, 0, 0);
    if (proxyShape == "Plane" && glm::all(glm::equal(normal, zeros))) {
        throw std::runtime_error("A vector defining the plane for planar projection must not be all zero.");
    }

    if (state.at(primitivePath / "attributes").contains("POSITION") == false) {
        throw std::runtime_error("Texture coordinate generation needs position attribute.");
    }

    nlohmann::json& posAccessor = state.at(getAccessor(state.at(primitivePath / "attributes" / "POSITION")));
    std::vector<glm::vec3> posVector = reactor::utils::getAccessorData<glm::vec3>(getAccessor(state.at(primitivePath / "attributes" / "POSITION")), state, datastorage);
    std::vector<glm::vec3> normalVector;
    if (state.at(primitivePath / "attributes").contains("NORMAL")) {
        normalVector = reactor::utils::getAccessorData<glm::vec3>(getAccessor(state.at(primitivePath / "attributes" / "NORMAL")), state, datastorage);
    }

    uint32_t vertexCount = posAccessor["count"];
    std::vector<glm::vec2> generatedTexCoords;
    generatedTexCoords.reserve(vertexCount);

    if (proxyShape == "Sphere") {
        TexCoordHelpers::createTexCoordsSpherical(generatedTexCoords, vertexCount, posVector, normalVector);
    } else if (proxyShape == "Cylinder") {
        TexCoordHelpers::createTexCoordsCylindrical(generatedTexCoords, vertexCount, posVector, normalVector);
    } else if (proxyShape == "Plane") {
        TexCoordHelpers::createTexCoordsPlanar(generatedTexCoords, vertexCount, posVector, normal);
    } else if (proxyShape == "Cube") {
        TexCoordHelpers::createTexCoordsCube(generatedTexCoords, vertexCount, posVector, normalVector);
    }

    // ------------------------- cleanup and storage ------------------
    auto attribute = std::string("TEXCOORD_") + std::to_string(texCoordIdx);
    if (state.at(primitivePath / "attributes").contains(attribute)) {
        auto accessorPath = getAccessor(state.at(primitivePath)["attributes"][attribute]);
        // first remove possibly already existing attribute
        state.at(primitivePath / "attributes").erase(attribute);

        { // Then remove no longer used accessor
            std::string op = "delete_accessor";
            auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
            taskDescription[op]["path"] = accessorPath.to_string();
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }
    }

    // finally recreate accessor and attribute
    auto accessorPathNew = reactor::utils::createAccessorFromData(generatedTexCoords, state, datastorage);
    state.at(primitivePath / "attributes")[attribute] = std::stoi(accessorPathNew.back());
}

float TexCoordHelpers::inclinationFromCartesian(glm::vec3 normal)
{
    // math see: https://en.wikipedia.org/wiki/Spherical_coordinate_system#Cylindrical_coordinates
    // the wikipedia article uses different coordinate axis than we do. -> resulting in fliped coordinates
    // x -> normal.z
    // y -> normal.x
    // z -> normal.y
    float r = glm::length(normal);
    if (r <= std::numeric_limits<float>::epsilon()) { // if normal is (0,0,0)
        r = std::numeric_limits<float>::epsilon();
    }
    return acosf(normal.y / r);
}

float TexCoordHelpers::azimuthFromCartesian(glm::vec3 normal)
{
    // math see: https://en.wikipedia.org/wiki/Spherical_coordinate_system#Cartesian_coordinates
    // the wikipedia article uses different coordinate axis than we do. -> resulting in fliped coordinates
    // x -> normal.z
    // y -> normal.x
    // z -> normal.y
    constexpr float pi = glm::pi<float>();
    float azimuth = 0.0f;
    if (normal.z > 0) {
        azimuth = atanf(normal.x / normal.z);
    } else if (normal.z < 0 && normal.x >= 0) {
        azimuth = atanf(normal.x / normal.z) + pi;
    } else if (normal.z < 0 && normal.x < 0) {
        azimuth = atanf(normal.x / normal.z) - pi;
    } else if (normal.z == 0 && normal.x > 0) {
        azimuth = pi / 2.0f;
    } else if (normal.z == 0 && normal.x < 0) {
        azimuth = -(pi / 2.0f);
    }
    return azimuth;
}

float TexCoordHelpers::inclinationFromSpherical(float inclination, glm::vec3 normal)
{
    float r = glm::length(normal);
    return r * sinf(inclination);
}

float TexCoordHelpers::rescale(float x, float origMin, float origMax, float targetMin, float targetMax)
{
    float result = ((targetMax - targetMin) * (x - origMin)) / (origMax - origMin) + targetMin;
    if (std::isnan(result)) {
        return 0.0f;
    }
    return result;
}

void TexCoordHelpers::createTexCoordsSpherical(std::vector<glm::vec2>& generatedTexCoords, uint32_t vertexCount, const std::vector<glm::vec3>& posData, const std::vector<glm::vec3>& normalData)
{
    constexpr float pi = glm::pi<float>();

    // generate texcoords
    for (uint32_t i = 0; i < vertexCount; i++) {

        glm::vec3 position = posData[i];
        glm::vec3 normal;
        if (normalData.size() > 0) {
            normal = normalData[i];
        } else {
            normal = position;
        }

        float inclination = TexCoordHelpers::inclinationFromCartesian(normal);
        float azimuth = TexCoordHelpers::azimuthFromCartesian(normal);
        generatedTexCoords.push_back(glm::vec2(TexCoordHelpers::rescale(inclination, 0, pi, 0, 1), 1 - TexCoordHelpers::rescale(azimuth, -pi, pi, 0, 1)));
        // 1-azimuth to flip texture
    }
}

void TexCoordHelpers::createTexCoordsCylindrical(std::vector<glm::vec2>& generatedTexCoords, uint32_t vertexCount, const std::vector<glm::vec3>& posData, const std::vector<glm::vec3>& normalData)
{
    constexpr float pi = glm::pi<float>();

    // generate texcoords
    for (uint32_t i = 0; i < vertexCount; i++) {
        glm::vec3 position = posData[i];
        glm::vec3 normal;
        if (normalData.size() > 0) {
            normal = normalData[i];
        } else {
            normal = position;
        }

        normal = glm::normalize(normal);

        float inclination = inclinationFromCartesian(normal);
        inclination = inclinationFromSpherical(inclination, normal);
        float azimuth = azimuthFromCartesian(normal);
        generatedTexCoords.push_back(glm::vec2(rescale(inclination, 0, pi, 0, 1), 1 - rescale(azimuth, -pi, pi, 0, 1)));
        // 1-azimuth to flip texture
    }
}

void TexCoordHelpers::createTexCoordsPlanar(std::vector<glm::vec2>& generatedTexCoords, uint32_t vertexCount, const std::vector<glm::vec3>& posData, glm::vec3 normal)
{
    CubeProjBin planeData;
    planeData.data.reserve(vertexCount);

    // generate texcoords
    for (uint32_t i = 0; i < vertexCount; i++) {
        glm::vec3 position = posData[i];
        glm::vec2 texCoord = calcTexCoordPlanar(position, normal);
        addTexCoordToCubeProjBin(planeData, texCoord, i);
    }

    // scale texcoords to correct range ([0, 1])
    fixScalesInCubeProjBin(planeData);

    for (CubeProjHelper& helper : planeData.data) {
        helper.texCoords.x = 1 - helper.texCoords.x; // fix for opengl texcoord mapping
        generatedTexCoords.push_back(helper.texCoords);
    }
}

void TexCoordHelpers::createTexCoordsCube(std::vector<glm::vec2>& generatedTexCoords, uint32_t vertexCount, const std::vector<glm::vec3>& posData, const std::vector<glm::vec3>& normalData)
{
    // create bins for all six cube sides
    CubeProjBin topData;
    topData.data.reserve(vertexCount / 6);
    CubeProjBin bottomData;
    bottomData.data.reserve(vertexCount / 6);
    CubeProjBin leftData;
    leftData.data.reserve(vertexCount / 6);
    CubeProjBin rightData;
    rightData.data.reserve(vertexCount / 6);
    CubeProjBin frontData;
    frontData.data.reserve(vertexCount / 6);
    CubeProjBin backData;
    backData.data.reserve(vertexCount / 6);

    // iterate vertices
    for (uint32_t vertIdx = 0; vertIdx < vertexCount; vertIdx++) {

        glm::vec3 position = posData[vertIdx];
        glm::vec3 normal;
        if (normalData.size() > 0) {
            normal = normalData[vertIdx];
        } else {
            normal = position;
        }

        // calculate texcoords
        CubeFace normalDir = getCubeFaceFromVector(normal);

        glm::vec2 texCoord;
        switch (normalDir) {
        case CubeFace::Top:
            texCoord = calcTexCoordPlanar(position, { 0, -1, 0 });
            addTexCoordToCubeProjBin(topData, texCoord, vertIdx);
            break;
        case CubeFace::Bottom:
            texCoord = calcTexCoordPlanar(position, { 0, 1, 0 });
            addTexCoordToCubeProjBin(bottomData, texCoord, vertIdx);
            break;
        case CubeFace::Left:
            texCoord = calcTexCoordPlanar(position, { 1, 0, 0 });
            addTexCoordToCubeProjBin(leftData, texCoord, vertIdx);
            break;
        case CubeFace::Right:
            texCoord = calcTexCoordPlanar(position, { -1, 0, 0 });
            addTexCoordToCubeProjBin(rightData, texCoord, vertIdx);
            break;
        case CubeFace::Front:
            texCoord = calcTexCoordPlanar(position, { 0, 0, -1 });
            addTexCoordToCubeProjBin(frontData, texCoord, vertIdx);
            break;
        case CubeFace::Back:
            texCoord = calcTexCoordPlanar(position, { 0, 0, 1 });
            addTexCoordToCubeProjBin(backData, texCoord, vertIdx);
            break;
        }
    }

    // fix scales in all bins
    fixScalesInCubeProjBin(topData);
    fixScalesInCubeProjBin(bottomData);
    fixScalesInCubeProjBin(leftData);
    fixScalesInCubeProjBin(rightData);
    fixScalesInCubeProjBin(frontData);
    fixScalesInCubeProjBin(backData);

    // combine tex coord data from all bins
    std::vector<CubeProjHelper> combinedTexCoordData;
    combinedTexCoordData.reserve(vertexCount);
    combinedTexCoordData.insert(
        combinedTexCoordData.end(),
        std::make_move_iterator(topData.data.begin()),
        std::make_move_iterator(topData.data.end()));
    combinedTexCoordData.insert(
        combinedTexCoordData.end(),
        std::make_move_iterator(bottomData.data.begin()),
        std::make_move_iterator(bottomData.data.end()));
    combinedTexCoordData.insert(
        combinedTexCoordData.end(),
        std::make_move_iterator(leftData.data.begin()),
        std::make_move_iterator(leftData.data.end()));
    combinedTexCoordData.insert(
        combinedTexCoordData.end(),
        std::make_move_iterator(rightData.data.begin()),
        std::make_move_iterator(rightData.data.end()));
    combinedTexCoordData.insert(
        combinedTexCoordData.end(),
        std::make_move_iterator(frontData.data.begin()),
        std::make_move_iterator(frontData.data.end()));
    combinedTexCoordData.insert(
        combinedTexCoordData.end(),
        std::make_move_iterator(backData.data.begin()),
        std::make_move_iterator(backData.data.end()));

    // sort data to realign with vertices
    std::sort(combinedTexCoordData.begin(), combinedTexCoordData.end(), [](const CubeProjHelper& a, const CubeProjHelper& b) { return a.vertIdx < b.vertIdx; });

    // set actual tex coords
    generatedTexCoords.clear();
    generatedTexCoords.reserve(vertexCount);
    for (CubeProjHelper& helper : combinedTexCoordData) {
        generatedTexCoords.push_back(helper.texCoords);
    }
}

glm::vec2 TexCoordHelpers::calcTexCoordPlanar(const glm::vec3& position, const glm::vec3& normal)
{
    glm::vec3 normalNormalized = glm::normalize(normal);
    glm::quat rotationQuat = glm::rotation(normalNormalized, glm::vec3(0, 0, 1));
    glm::vec3 rotatedPos = glm::rotate(rotationQuat, position);
    return glm::vec2(rotatedPos.x, rotatedPos.y);
}

TexCoordHelpers::CubeFace TexCoordHelpers::getCubeFaceFromVector(const glm::vec3& normal)
{
    // get angle between normal and all six cube surface normals
    float up = angleBetweenVectors(normal, glm::vec3(0, 1, 0));
    float down = angleBetweenVectors(normal, glm::vec3(0, -1, 0));
    float front = angleBetweenVectors(normal, glm::vec3(0, 0, 1));
    float back = angleBetweenVectors(normal, glm::vec3(0, 0, -1));
    float right = angleBetweenVectors(normal, glm::vec3(1, 0, 0));
    float left = angleBetweenVectors(normal, glm::vec3(-1, 0, 0));

    // find smallest angle and return respective cube face
    // if top smallest
    if (up <= front && up <= back && up <= left && up <= right && up < down) {
        return CubeFace::Top;
    }

    // if down smallest
    else if (down <= front && down <= back && down <= left && down <= right && down < up) {
        return CubeFace::Bottom;
    }

    // if front smallest
    else if (front < up && front < down && front < left && front < right && front < back) {
        return CubeFace::Front;
    }

    // if back smallest
    else if (back < up && back < down && back < left && back < right && back < front) {
        return CubeFace::Back;
    }

    // if left smallest
    else if (left < up && left < down && left < front && left < back && left < right) {
        return CubeFace::Left;
    }

    // if left smallest
    else if (right < up && right < down && right < front && right < back && right < left) {
        return CubeFace::Right;
    }

    return CubeFace::Top;
}

float TexCoordHelpers::angleBetweenVectors(const glm::vec3& a, const glm::vec3& b)
{
    glm::vec3 aNormalized = glm::normalize(a);
    glm::vec3 bNormalized = glm::normalize(b);
    return glm::acos(glm::dot(aNormalized, bNormalized));
}

void TexCoordHelpers::addTexCoordToCubeProjBin(CubeProjBin& bin, glm::vec2 texCoord, uint32_t vertIdx)
{
    CubeProjHelper helper(vertIdx, texCoord);
    bin.data.push_back(helper);

    // update min and max values
    if (bin.minX > texCoord.x) {
        bin.minX = texCoord.x;
    }
    if (bin.maxX < texCoord.x) {
        bin.maxX = texCoord.x;
    }
    if (bin.minY > texCoord.y) {
        bin.minY = texCoord.y;
    }
    if (bin.maxY < texCoord.y) {
        bin.maxY = texCoord.y;
    }
}

void TexCoordHelpers::fixScalesInCubeProjBin(CubeProjBin& bin)
{
    // scale texcoords to correct range ([0, 1])
    for (uint32_t i = 0; i < bin.data.size(); i++) {
        bin.data[i].texCoords.x = 1 - rescale(bin.data[i].texCoords.x, bin.minX, bin.maxX, 0, 1); // 1- value to flip values
        bin.data[i].texCoords.y = 1 - rescale(bin.data[i].texCoords.y, bin.minY, bin.maxY, 0, 1); // 1- value to flip values
    }
}

bool TexCoordHelpers::canCreateTexCoord(uint32_t idx, const nlohmann::json& primitive)
{
    int largestIdx = -1;
    // find largest tex coord
    for (const auto& [key, value] : primitive["attributes"].items()) {
        if (key.find("TEXCOORD_") != std::string::npos) {
            std::string primAttrNumberStr = key.substr(9); // get everything in name after "TEXCOORD_"
            if (largestIdx < std::stoi(primAttrNumberStr)) {
                largestIdx = std::stoi(primAttrNumberStr);
            }
        }
    }

    if ((int)idx - 1 <= largestIdx) {
        return true;
    } else {
        return false;
    }
}
