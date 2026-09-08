#include "gltf/SpecularGlossinessConverter.h"
#include "gltf/Gltf.h"
#include "gltf/Image.h"
#include "gltf/Material.h"
#include "gltf/Texture.h"
#include "imageformats/imageformats.h"
#include "math/VectorFunctions.h"

using namespace ux3d;
using namespace ux3d::gltf;

glm::dvec3 SpecularGlossinessConverter::m_dielectricSpecular = { 0.04, 0.04, 0.04 };
double SpecularGlossinessConverter::m_srgbGamma = 2.2;
double SpecularGlossinessConverter::m_epsilon = 1e-6;

SpecularGlossinessConverter::SpecularGlossinessConverter(Gltf& _gltf, Material* _material)
    : m_gltf(_gltf)
    , m_material(_material)
{
}

GltfResult SpecularGlossinessConverter::convert()
{
    if (m_material == nullptr || m_material->getPbrSpecularGlossiness().has_value() == false) {
        return GltfError::InvalidInput;
    }

    // get existing KHR_materials_pbrSpecularGlossiness data
    PbrSpecularGlossiness& specularGlossiness = m_material->getPbrSpecularGlossiness().value();
    const glm::dvec4 diffuseFactor = specularGlossiness.getDiffuseFactorOrDefault();
    const glm::dvec3 specularFactor = specularGlossiness.getSpecularFactorOrDefault();
    const double glossinessFactor = specularGlossiness.getGlossinessFactorOrDefault();

    bool hasDiffuseImage = false;
    bool diffuseImageIsValid = true; // no image is also valid
    bool diffuseHasAlpha = false;
    uint32_t diffuseImageWidth = 0;
    uint32_t diffuseImageHeight = 0;
    std::optional<std::vector<uint8_t>> diffusePixels;
    if (specularGlossiness.getDiffuseTextureInfo().has_value()) {
        TextureInfo& diffuseTexInfo = specularGlossiness.getDiffuseTextureInfo().value();
        m_diffuseTexture = diffuseTexInfo.getTexture();

        if (m_diffuseTexture != nullptr) {
            m_diffuseImage = m_diffuseTexture->getSource();

            if (m_diffuseImage != nullptr) {
                if (diffuseTexInfo.hasTextureTransform()) {
                    return gltf::GltfError::TextureTransformIsPresent;
                }
                const image::Image& diffuseImageData = m_diffuseImage->getData();
                diffusePixels.emplace(diffuseImageData.getPixels());

                hasDiffuseImage = true;

                diffuseImageWidth = diffuseImageData.getWidth();
                diffuseImageHeight = diffuseImageData.getHeight();

                const uint32_t channelCount = diffuseImageData.getChannelCount();
                diffuseImageIsValid = channelCount == 3 || channelCount == 4;
                diffuseHasAlpha = channelCount == 4;
            }
        }
    }

    bool hasSpecularGlossinessImage = false;
    bool specularGlossinessImageIsValid = true; // no image is also valid
    uint32_t specularGlossinessImageWidth = 0;
    uint32_t specularGlossinessImageHeight = 0;
    std::optional<std::vector<uint8_t>> specularGlossinessPixels;
    if (specularGlossiness.getSpecularGlossinessTextureInfo().has_value()) {
        TextureInfo& specGlossinessTexInfo = specularGlossiness.getSpecularGlossinessTextureInfo().value();
        m_specularGlossinessTexture = specGlossinessTexInfo.getTexture();

        if (m_specularGlossinessTexture != nullptr) {
            m_specularGlossinessImage = m_specularGlossinessTexture->getSource();

            if (m_specularGlossinessImage != nullptr) {
                if (specGlossinessTexInfo.hasTextureTransform()) {
                    return gltf::GltfError::TextureTransformIsPresent;
                }
                const image::Image& specularGlossinessImageData = m_specularGlossinessImage->getData();
                specularGlossinessPixels.emplace(specularGlossinessImageData.getPixels());

                hasSpecularGlossinessImage = true;

                specularGlossinessImageWidth = specularGlossinessImageData.getWidth();
                specularGlossinessImageHeight = specularGlossinessImageData.getHeight();

                specularGlossinessImageIsValid = specularGlossinessImageData.getChannelCount() == 4;
            }
        }
    }

    // determine whether texture conversion is needed/possible
    bool textureConversionNeeded = false;
    uint32_t imageWidth = 0;
    uint32_t imageHeight = 0;
    if (hasDiffuseImage) {
        if (diffuseImageIsValid && specularGlossinessImageIsValid) {
            if (hasSpecularGlossinessImage) {
                if (diffuseImageWidth == specularGlossinessImageWidth && diffuseImageHeight == specularGlossinessImageHeight) {
                    textureConversionNeeded = true;
                    imageWidth = diffuseImageWidth;
                    imageHeight = diffuseImageHeight;
                } else {
                    return GltfError::ResolutionMismatch;
                }
            } else {
                textureConversionNeeded = true;
                imageWidth = diffuseImageWidth;
                imageHeight = diffuseImageHeight;
            }
        } else {
            return GltfError::InvalidInput;
        }
    } else {
        if (hasSpecularGlossinessImage) {
            if (specularGlossinessImageIsValid) {
                textureConversionNeeded = true;
                imageWidth = specularGlossinessImageWidth;
                imageHeight = specularGlossinessImageHeight;
            } else {
                return GltfError::InvalidInput;
            }
        }
    }

    // convert factors (everything except the images)
    MetallicRoughnessData convertedData = convertData(diffuseFactor, specularFactor, glossinessFactor);

    // convert images
    if (textureConversionNeeded) {
        const size_t imageSize = static_cast<size_t>(imageWidth) * static_cast<size_t>(imageHeight);

        // prepare data containers for conversion results
        core::Vector<uint8_t> baseColorPixels;
        baseColorPixels.resize(imageSize * 4);
        core::Vector<uint8_t> metallicRoughnessPixels;
        metallicRoughnessPixels.resize(imageSize * 4);
        core::Vector<uint8_t> specularPixels;
        specularPixels.resize(imageSize * 3);

        size_t loopCount = 0;
        for (size_t i = 0; i < imageSize * 4; i += 4) {
            glm::dvec4 diffuseVector;
            if (diffusePixels.has_value()) {
                if (diffuseHasAlpha) { // this handles diffuse RGBA and RGB textures by adding an alpha of 1.0 to the RGB values
                    diffuseVector = { diffusePixels.value()[i] / 255.0, diffusePixels.value()[i + 1] / 255.0, diffusePixels.value()[i + 2] / 255.0, diffusePixels.value()[i + 3] / 255.0 };
                } else {
                    diffuseVector = { diffusePixels.value()[i - loopCount] / 255.0, diffusePixels.value()[i - loopCount + 1] / 255.0, diffusePixels.value()[i - loopCount + 2] / 255.0, 1.0 };
                }
                diffuseVector = toLinear(diffuseVector);
            } else {
                // use the diffuseFactor in case that no diffuse image is present
                diffuseVector = diffuseFactor;
            }

            glm::dvec3 specularVector;
            double glossinessValue;
            if (specularGlossinessPixels.has_value()) {
                specularVector = { specularGlossinessPixels.value()[i] / 255.0, specularGlossinessPixels.value()[i + 1] / 255.0, specularGlossinessPixels.value()[i + 2] / 255.0 };
                specularVector = toLinear(specularVector);
                // apply the glossinessFactor here as well and later use roughnessFactor = 1, to encode glossinessFactor directly into the texture
                glossinessValue = (specularGlossinessPixels.value()[i + 3] / 255.0) * glossinessFactor;
            } else {
                // use these factors in case that no specularGlossiness image is present
                specularVector = specularFactor;
                glossinessValue = glossinessFactor;
            }

            // convert
            MetallicRoughnessData convertedImageData = convertData(diffuseVector, specularVector, glossinessValue);
            convertedImageData.baseColor = toSrgb(convertedImageData.baseColor);
            const glm::dvec3 specularColorVector = toSrgb(specularVector);

            // save converted data as normalized uint8_t
            baseColorPixels[i + 0] = static_cast<uint8_t>(convertedImageData.baseColor.r * 255.0);
            baseColorPixels[i + 1] = static_cast<uint8_t>(convertedImageData.baseColor.g * 255.0);
            baseColorPixels[i + 2] = static_cast<uint8_t>(convertedImageData.baseColor.b * 255.0);
            baseColorPixels[i + 3] = static_cast<uint8_t>(convertedImageData.baseColor.a * 255.0);

            metallicRoughnessPixels[i + 0] = 0; // red channel
            metallicRoughnessPixels[i + 1] = static_cast<uint8_t>(convertedImageData.roughnessFactor * 255.0); // green channel
            metallicRoughnessPixels[i + 2] = static_cast<uint8_t>(convertedImageData.metallicFactor * 255.0); // blue channel
            metallicRoughnessPixels[i + 3] = 255; // alpha channel

            specularPixels[i - loopCount + 0] = static_cast<uint8_t>(specularColorVector.r * 255.0);
            specularPixels[i - loopCount + 1] = static_cast<uint8_t>(specularColorVector.g * 255.0);
            specularPixels[i - loopCount + 2] = static_cast<uint8_t>(specularColorVector.b * 255.0);

            loopCount++;
        }

        // RoughnessFactor = 1.0, because in this case we multiply the glossinessFactor into the texture,
        // to fix the issue of e.g. GlossinessFactor = 1.0, GlossinessTexture is present => RoughnessFactor = 0.0, RoughnessTexture is present (but ignored)
        convertedData.roughnessFactor = 1.0;

        image::Image baseColorData;
        baseColorData.initializeLayout(8, 8, 8, 8, image::Format::NORM, false, true);
        baseColorData.initialize(&baseColorPixels[0], imageSize * 4, imageWidth, imageHeight);

        image::Image metallicRoughnessData;
        metallicRoughnessData.initializeLayout(8, 8, 8, 8, image::Format::NORM, false, true);
        metallicRoughnessData.initialize(&metallicRoughnessPixels[0], imageSize * 4, imageWidth, imageHeight);

        image::Image specularData;
        specularData.initializeLayout(8, 8, 8, 0, image::Format::NORM, false, true);
        specularData.initialize(&specularPixels[0], imageSize * 3, imageWidth, imageHeight);

        // encode pixel data to write in image buffers
        core::Vector<uint8_t> baseColorPixelsPNG;
        if (core::Result::Success != imageformats::encodePNG(baseColorData, baseColorPixelsPNG)) {
            return GltfError::EncodingFailed;
        }

        core::Vector<uint8_t> metallicRoughnessPixelsPNG;
        if (core::Result::Success != imageformats::encodePNG(metallicRoughnessData, metallicRoughnessPixelsPNG)) {
            return GltfError::EncodingFailed;
        }

        core::Vector<uint8_t> specularPixelsPNG;
        if (core::Result::Success != imageformats::encodePNG(specularData, specularPixelsPNG)) {
            return GltfError::EncodingFailed;
        }

        core::Vector<uint8_t> pixelDataPNG;
        pixelDataPNG.resize(baseColorPixelsPNG.size() + metallicRoughnessPixelsPNG.size() + specularPixelsPNG.size());

        for (size_t j = 0; j < baseColorPixelsPNG.size(); ++j) {
            pixelDataPNG[j] = baseColorPixelsPNG[j];
        }

        for (size_t k = 0; k < metallicRoughnessPixelsPNG.size(); ++k) {
            pixelDataPNG[baseColorPixelsPNG.size() + k] = metallicRoughnessPixelsPNG[k];
        }

        for (size_t l = 0; l < specularPixelsPNG.size(); ++l) {
            pixelDataPNG[baseColorPixelsPNG.size() + metallicRoughnessPixelsPNG.size() + l] = specularPixelsPNG[l];
        }

        // copy generated data into a new Buffer
        Buffer* buffer = m_gltf.create<Buffer>();
        buffer->setStorageFormat(StorageFormat::File);
        buffer->setByteLength(static_cast<uint32_t>(pixelDataPNG.size()));
        buffer->setData(pixelDataPNG.getMemoryRange());

        // create BufferViews on that Buffer
        BufferView* baseColorBufferView = m_gltf.create<BufferView>();
        baseColorBufferView->setBuffer(*buffer);
        baseColorBufferView->setByteLength(static_cast<uint32_t>(baseColorPixelsPNG.size()));
        baseColorBufferView->setByteOffset(0);
        baseColorBufferView->setByteStride(Accessor::getComponentCountFrom(AttributeType::Vec4));

        BufferView* metallicRoughnessBufferView = m_gltf.create<BufferView>();
        metallicRoughnessBufferView->setBuffer(*buffer);
        metallicRoughnessBufferView->setByteLength(static_cast<uint32_t>(metallicRoughnessPixelsPNG.size()));
        metallicRoughnessBufferView->setByteOffset(static_cast<uint32_t>(baseColorPixelsPNG.size()));
        metallicRoughnessBufferView->setByteStride(Accessor::getComponentCountFrom(AttributeType::Vec4));

        BufferView* specularBufferView = m_gltf.create<BufferView>();
        specularBufferView->setBuffer(*buffer);
        specularBufferView->setByteLength(static_cast<uint32_t>(specularPixelsPNG.size()));
        specularBufferView->setByteOffset(static_cast<uint32_t>(baseColorPixelsPNG.size() + metallicRoughnessPixelsPNG.size()));
        specularBufferView->setByteStride(Accessor::getComponentCountFrom(AttributeType::Vec3));

        // create textures and images
        m_baseColorTexture = m_gltf.create<Texture>();
        m_metallicRoughnessTexture = m_gltf.create<Texture>();
        m_specularColorTexture = m_gltf.create<Texture>();

        m_baseColorImage = m_gltf.create<Image>();
        m_metallicRoughnessImage = m_gltf.create<Image>();
        m_specularColorImage = m_gltf.create<Image>();

        m_baseColorImage->setMimeType(imageformats::ImageEncoding::PNG);
        m_metallicRoughnessImage->setMimeType(imageformats::ImageEncoding::PNG);
        m_specularColorImage->setMimeType(imageformats::ImageEncoding::PNG);

        // set buffer views for images
        m_baseColorImage->setBufferView(*baseColorBufferView);
        m_metallicRoughnessImage->setBufferView(*metallicRoughnessBufferView);
        m_specularColorImage->setBufferView(*specularBufferView);

        // update image data
        m_baseColorImage->loadDataToMemory({});
        m_metallicRoughnessImage->loadDataToMemory({});
        m_specularColorImage->loadDataToMemory({});

        // use images in new textures
        m_baseColorTexture->setSource(*m_baseColorImage);
        m_metallicRoughnessTexture->setSource(*m_metallicRoughnessImage);
        m_specularColorTexture->setSource(*m_specularColorImage);
    }

    // destroy possibly existing objects that need to be overwritten
    if (m_material->getPbrMetallicRoughness().has_value()) {
        m_material->destroyPbrMetallicRoughness();
    }
    if (m_material->getSpecular().has_value()) {
        m_material->destroySpecular();
    }
    if (m_material->getIor().has_value()) {
        m_material->destroyIor();
    }

    // create metallic roughness
    m_material->createPbrMetallicRoughness();
    PbrMetallicRoughness& metallicRoughness = m_material->getPbrMetallicRoughness().value();

    metallicRoughness.setMetallicFactor(convertedData.metallicFactor);
    metallicRoughness.setRoughnessFactor(convertedData.roughnessFactor);
    metallicRoughness.setBaseColorFactor(convertedData.baseColor);

    if (m_baseColorTexture != nullptr && m_metallicRoughnessTexture != nullptr) {
        metallicRoughness.createBaseColorTextureInfo();
        metallicRoughness.createMetallicRoughnessTextureInfo();
        TextureInfo& baseColorTextureInfo = metallicRoughness.getBaseColorTextureInfo().value();
        TextureInfo& metallicRoughnessTextureInfo = metallicRoughness.getMetallicRoughnessTextureInfo().value();
        baseColorTextureInfo.setTexture(*m_baseColorTexture);
        metallicRoughnessTextureInfo.setTexture(*m_metallicRoughnessTexture);
    }

    // create KHR_materials_specular extension
    m_material->createSpecular();
    Specular& specular = m_material->getSpecular().value();
    specular.setSpecularColorFactor(specularFactor);

    if (m_specularColorTexture != nullptr) {
        specular.createSpecularColorTextureInfo();
        TextureInfo& specularColorTextureInfo = specular.getSpecularColorTextureInfo().value();
        specularColorTextureInfo.setTexture(*m_specularColorTexture);
    }

    // create KHR_materials_ior extension
    m_material->createIor();
    Ior& ior = m_material->getIor().value();
    ior.setIor(0.0);

    return GltfResult::Success;
}

void SpecularGlossinessConverter::cleanup()
{
    m_material->destroyPbrSpecularGlossiness();

    bool deleteDiffuseTexture = true;
    bool deleteSpecularGlossinessTexture = true;

    core::Vector<TextureInfo*> textureInfos = m_gltf.getTextureInfos();
    for (TextureInfo* textureInfo : textureInfos) {
        if (textureInfo == nullptr) {
            continue;
        }
        if (textureInfo->getTexture() == m_diffuseTexture) {
            deleteDiffuseTexture = false;
        }
        if (textureInfo->getTexture() == m_specularGlossinessTexture) {
            deleteSpecularGlossinessTexture = false;
        }
    }
    if (deleteDiffuseTexture) {
        m_gltf.deleteTexture(*m_diffuseTexture);
    }
    if (deleteSpecularGlossinessTexture) {
        m_gltf.deleteTexture(*m_specularGlossinessTexture);
    }

    bool deleteDiffuseImage = true;
    bool deleteSpecularGlossinessImage = true;

    core::LinkedList<Texture>& textures = m_gltf.getTextures();
    for (Texture& texture : textures) {
        Image* image = texture.getSource();
        if (image == m_diffuseImage) {
            deleteDiffuseImage = false;
        }
        if (image == m_specularGlossinessImage) {
            deleteSpecularGlossinessImage = false;
        }
    }
    if (deleteDiffuseImage) {
        m_gltf.deleteImage(*m_diffuseImage);
    }
    if (deleteSpecularGlossinessImage) {
        m_gltf.deleteImage(*m_specularGlossinessImage);
    }
}

ChangedProperties SpecularGlossinessConverter::getCreatedAndDestroyedProperties()
{
    ChangedProperties changes;
    // deleted
    if (m_diffuseTexture != nullptr) {
        changes.deletedTextures.push_back(m_diffuseTexture);
    }
    if (m_specularGlossinessTexture != nullptr) {
        changes.deletedTextures.push_back(m_specularGlossinessTexture);
    }
    if (m_diffuseImage != nullptr) {
        changes.deletedImages.push_back(m_diffuseImage);
    }
    if (m_specularGlossinessImage != nullptr) {
        changes.deletedImages.push_back(m_specularGlossinessImage);
    }

    // created
    if (m_baseColorTexture != nullptr) {
        changes.createdTextures.push_back(m_baseColorTexture);
    }
    if (m_metallicRoughnessTexture != nullptr) {
        changes.createdTextures.push_back(m_metallicRoughnessTexture);
    }
    if (m_specularGlossinessTexture != nullptr) {
        changes.createdTextures.push_back(m_specularGlossinessTexture);
    }
    if (m_baseColorImage != nullptr) {
        changes.createdImages.push_back(m_baseColorImage);
    }
    if (m_metallicRoughnessImage != nullptr) {
        changes.createdImages.push_back(m_metallicRoughnessImage);
    }
    if (m_specularColorImage != nullptr) {
        changes.createdImages.push_back(m_specularColorImage);
    }

    return changes;
}

MetallicRoughnessData SpecularGlossinessConverter::convertData(const glm::dvec4 diffuseVector, const glm::dvec3 specularVector, const double glossinessValue)
{
    const glm::dvec3 diffuseColor(diffuseVector.r, diffuseVector.g, diffuseVector.b);
    const double oneMinusSpecularStrength = 1 - getMaxComponent(specularVector);
    const double metallicFactor = solveMetallic(getPerceivedBrightness(diffuseColor), getPerceivedBrightness(specularVector), oneMinusSpecularStrength, m_dielectricSpecular.r);

    const glm::dvec3 baseColorFromDiffuse = diffuseColor * (oneMinusSpecularStrength / (1 - m_dielectricSpecular.r) / math::max(1 - metallicFactor, m_epsilon));
    const glm::dvec3 baseColorFromSpecular = (specularVector - (m_dielectricSpecular * (1 - metallicFactor))) * (1 / math::max(metallicFactor, m_epsilon));
    glm::dvec3 baseColor = lerp(baseColorFromDiffuse, baseColorFromSpecular, metallicFactor * metallicFactor);
    baseColor = math::clamp(baseColor, 0.0, 1.0);

    const double roughnessFactor = 1 - glossinessValue;

    MetallicRoughnessData results;
    results.baseColor = glm::dvec4(baseColor, diffuseVector.a);
    results.metallicFactor = metallicFactor;
    results.roughnessFactor = roughnessFactor;

    return results;
}

double SpecularGlossinessConverter::getMaxComponent(const glm::dvec3& _vector)
{
    return math::max(math::max(_vector.r, _vector.g), _vector.b);
}

double SpecularGlossinessConverter::solveMetallic(const double _diffuseBrightness, const double _specularBrightness, const double _oneMinusSpecularStrength, const double _dielectricSpecular)
{
    if (_specularBrightness < _dielectricSpecular) {
        return 0.0;
    }

    const double a = _dielectricSpecular;
    const double b = _diffuseBrightness * _oneMinusSpecularStrength / (1 - _dielectricSpecular) + _specularBrightness - 2 * _dielectricSpecular;
    const double c = _dielectricSpecular - _specularBrightness;
    const double d = math::max(b * b - 4 * a * c, 0.0);
    return math::clamp((-b + math::sqrt(d)) / (2 * a), 0.0, 1.0);
}

double SpecularGlossinessConverter::getPerceivedBrightness(const glm::dvec3& _vector)
{
    return math::sqrt(0.299 * _vector.r * _vector.r + 0.587 * _vector.g * _vector.g + 0.114 * _vector.b * _vector.b);
}

glm::dvec3 SpecularGlossinessConverter::lerp(const glm::dvec3& a, const glm::dvec3& b, const double ratio)
{
    return a * (1 - ratio) + b * ratio;
}

math::dvec4 SpecularGlossinessConverter::toLinear(const math::dvec4& _colorVector)
{
    return math::dvec4(toLinear(math::dvec3(_colorVector)), _colorVector.a);
}

math::dvec3 SpecularGlossinessConverter::toLinear(const math::dvec3& _colorVector)
{
    return math::dvec3(pow(_colorVector[0], m_srgbGamma), pow(_colorVector[1], m_srgbGamma), pow(_colorVector[2], m_srgbGamma));
}

math::dvec4 SpecularGlossinessConverter::toSrgb(const math::dvec4& _colorVector)
{
    return math::dvec4(toSrgb(math::dvec3(_colorVector)), _colorVector.a);
}

math::dvec3 SpecularGlossinessConverter::toSrgb(const math::dvec3& _colorVector)
{
    return math::dvec3(pow(_colorVector[0], 1.0 / m_srgbGamma), pow(_colorVector[1], 1.0 / m_srgbGamma), pow(_colorVector[2], 1.0 / m_srgbGamma));
}
