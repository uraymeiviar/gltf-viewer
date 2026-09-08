#pragma once
#include "GltfResult.h"
#include "core/Vector.h"
#include "math/VectorTypes.h"

namespace ux3d::gltf {
class Gltf;
class Material;
class Texture;
class Image;

struct ChangedProperties {
    core::Vector<Texture*> deletedTextures;
    core::Vector<Texture*> createdTextures;
    core::Vector<Image*> deletedImages;
    core::Vector<Image*> createdImages;
};

struct MetallicRoughnessData {
    glm::dvec4 baseColor = {};
    double metallicFactor = 0.0;
    double roughnessFactor = 0.0;
};

class SpecularGlossinessConverter {
public:
    SpecularGlossinessConverter(Gltf& _gltf, gltf::Material* material);
    GltfResult convert();
    void cleanup();
    ChangedProperties getCreatedAndDestroyedProperties();

private:
    Gltf& m_gltf;
    Material* m_material = nullptr;

    Texture* m_diffuseTexture = nullptr;
    Texture* m_specularGlossinessTexture = nullptr;
    Image* m_diffuseImage = nullptr;
    Image* m_specularGlossinessImage = nullptr;

    Texture* m_baseColorTexture = nullptr;
    Texture* m_metallicRoughnessTexture = nullptr;
    Texture* m_specularColorTexture = nullptr;
    Image* m_baseColorImage = nullptr;
    Image* m_metallicRoughnessImage = nullptr;
    Image* m_specularColorImage = nullptr;

    static glm::dvec3 m_dielectricSpecular;
    static double m_epsilon;
    static double m_srgbGamma;

    MetallicRoughnessData convertData(const glm::dvec4 diffuseVector, const glm::dvec3 specularVector, const double glossinessValue);
    static double getMaxComponent(const glm::dvec3& _vector);
    static double solveMetallic(const double _diffuseBrightness, const double _specularBrightness, const double _oneMinusSpecularStrength, const double _dielectricSpecular);
    static double getPerceivedBrightness(const glm::dvec3& _vector);
    static glm::dvec3 lerp(const glm::dvec3& a, const glm::dvec3& b, const double ratio);
    static math::dvec4 toLinear(const math::dvec4& _colorVector);
    static math::dvec3 toLinear(const math::dvec3& _colorVector);
    static math::dvec4 toSrgb(const math::dvec4& _colorVector);
    static math::dvec3 toSrgb(const math::dvec3& _colorVector);
};
}
