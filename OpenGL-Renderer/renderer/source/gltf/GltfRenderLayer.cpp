// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gltf/GltfRenderLayer.h"
#include "animation/Animation.h"
#include "animation/Sampler.h"
#include "core/Result.h"
#include "imageformats/imageformats.h"
#include "renderer/CommandGenerator.h"
#include "renderer/CommandMapping.h"
#include "renderer/EnvironmentMap.h"
#include "renderer/Renderer.h"
#include "renderer/editor/EditorRenderLayer.h"
#include "renderer/gl/Attribute.h"
#include "renderer/gl/Util.h"
#include "renderer/passes/OpaquePass.h"
#include "renderer/passes/TransmissivePass.h"
#include "renderer/passes/TransparentPass.h"

#include <functional>
#include <chrono>
#include <memory>
#include <vector>

using namespace ux3d;
using namespace ux3d::renderer;

namespace ux3d::renderer {

bool convert(const gltf::Accessor& accessor, std::vector<float>& output, bool applyPadding)
{
    core::Slice<uint8_t> rawData;
    if (accessor.getSparse().has_value()) {
        if (!accessor.getSparseDataAs(rawData)) {
            return false;
        }
    } else if (!accessor.getDataAs(rawData)) {
        return false;
    }

    core::MemoryRange<uint8_t> input = rawData.getMemoryRange();
    gltf::ComponentType componentType = accessor.getComponentType();
    gltf::AttributeType attributeType = accessor.getType();
    uint32_t componentCount = accessor.getComponentCountFrom(attributeType);
    size_t count = accessor.getCount();

    size_t paddingBytes = accessor.getByteLengthFrom(componentType, attributeType, true) - accessor.getByteLengthFrom(componentType, attributeType, false);
    size_t componentByteLength = accessor.getByteLengthFrom(componentType);
    size_t paddingElements = 0; // padding per attribute measured in component width units
    if (applyPadding) {
        paddingElements = paddingBytes / componentByteLength;
    }

    bool isNormalized = accessor.isNormalized();

    output.reserve(input.size());

    auto convertData = [&](auto dataView, auto func) -> void {
        for (size_t i = 0; i < count * (componentCount + paddingElements); i += componentCount + paddingElements) {
            for (size_t componentIdx = 0; componentIdx < componentCount; componentIdx++) {
                output.emplace_back(func(static_cast<float>(dataView[i + componentIdx])));
            }
            for (size_t paddingIdx = 0; paddingIdx < paddingElements; paddingIdx++) {
                // padding
            }
        }
    };

    std::function<float(float)> func;
    if (isNormalized) {
        switch (componentType) {
        case gltf::ComponentType::UnsignedByte:
            func = [](float c) -> float { return c / 255.f; };
            break;
        case gltf::ComponentType::Byte:
            func = [](float c) -> float { return std::max(c / 127.f, -1.f); };
            break;
        case gltf::ComponentType::Short:
            func = [](float c) -> float { return std::max(c / 32767.f, -1.f); };
            break;
        case gltf::ComponentType::UnsignedShort:
            func = [](float c) -> float { return c / 65535.f; };
            break;
        default:
            break;
        }
    } else {
        func = [](float c) -> float { return c; };
    }

    switch (componentType) {
    case gltf::ComponentType::UnsignedByte:
        convertData(input.viewAs<const uint8_t>(), func);
        return true;
    case gltf::ComponentType::Byte:
        convertData(input.viewAs<const int8_t>(), func);
        return true;
    case gltf::ComponentType::Short:
        convertData(input.viewAs<const int16_t>(), func);
        return true;
    case gltf::ComponentType::UnsignedShort:
        convertData(input.viewAs<const uint16_t>(), func);
        return true;
    default:
        break;
    }

    return false;
}

template <class T, typename Sampler = typename animation::Channel<std::vector<T>>::SamplerFunction>
bool createChannel(Sampler func, animation::IAnimationTarget<std::vector<T>>& animationTarget, const gltf::AnimationChannel& gltfAnimationChannel, animation::Animation& animation)
{
    auto sampler = gltfAnimationChannel.getSampler();
    auto interpolation = sampler->getInterpolation();
    auto inputAccessor = sampler->getInputAccessor();
    auto outputAccessor = sampler->getOutputAccessor();

    const uint32_t keyCount = inputAccessor->getCount();

    animation::Channel<std::vector<T>> channel(animationTarget, func);

    core::Slice<float> keys;
    if (inputAccessor->getSparse().has_value()) {
        if (!inputAccessor->getSparseDataAs(keys)) {
            return false;
        }
    } else if (!inputAccessor->getDataAs(keys)) {
        return false;
    }

    if (keys.size() != keyCount) {
        return false;
    }

    core::Slice<uint8_t> rawData;
    if (outputAccessor->getSparse().has_value()) {
        if (!outputAccessor->getSparseDataAs(rawData)) {
            return false;
        }
    } else if (!outputAccessor->getDataAs(rawData)) {
        return false;
    }

    core::MemoryRange<uint8_t> outputData = rawData.getMemoryRange();

    std::vector<float> conversionData;
    if (outputAccessor->getComponentType() != gltf::ComponentType::Float) {
        if (convert(*outputAccessor, conversionData, false) == false) {
            return false;
        }
        outputData = core::makeMutableByteRange(conversionData).viewAs<uint8_t>();
    }

    const auto outSize = outputData.size();

    uint32_t valueCount = static_cast<uint32_t>(outSize) / keyCount / sizeof(T);

    std::vector<T> values, inTangents, outTangents;

    if (interpolation == gltf::AnimationInterpolation::CubicSpline) {
        valueCount /= 3u;
        inTangents.resize(valueCount);
        outTangents.resize(valueCount);
    }
    values.resize(valueCount);

    auto outputValues = reinterpret_cast<const T*>(outputData.data());

    for (uint32_t i = 0; i < keyCount; ++i) {
        auto offset = i * valueCount;

        if (interpolation != gltf::AnimationInterpolation::CubicSpline) {
            for (uint32_t v = 0u; v < valueCount; ++v) {
                values[v] = outputValues[offset + v];
            }
            channel.addKeyframe(keys[i], values);
        } else {
            for (uint32_t v = 0u; v < valueCount; ++v) {
                inTangents[v] = outputValues[offset + v];
                values[v] = outputValues[offset + v + 1];
                outTangents[v] = outputValues[offset + v + 2];
            }
            channel.addKeyframe(keys[i], inTangents, values, outTangents);
        }
    }

    animation.channels.emplace_back(std::move(channel));
    return true;
}

template <class T, typename Channel = typename animation::Channel<T>, typename Sampler = typename Channel::SamplerFunction>
bool createChannel(Sampler func, animation::IAnimationTarget<T>& animationTarget, const gltf::AnimationChannel& gltfAnimationChannel, animation::Animation& animation)
{
    auto sampler = gltfAnimationChannel.getSampler();
    auto interpolation = sampler->getInterpolation();
    auto inputAccessor = sampler->getInputAccessor();
    auto outputAccessor = sampler->getOutputAccessor();

    const uint32_t keyCount = inputAccessor->getCount();

    Channel channel(animationTarget, func);

    core::Slice<float> keys;
    if (inputAccessor->getSparse().has_value()) {
        if (!inputAccessor->getSparseDataAs(keys)) {
            return false;
        }
    } else if (!inputAccessor->getDataAs(keys)) {
        return false;
    }

    if (keys.size() != keyCount) {
        return false;
    }

    core::Slice<uint8_t> rawData;
    if (outputAccessor->getSparse().has_value()) {
        if (!outputAccessor->getSparseDataAs(rawData)) {
            return false;
        }
    } else if (!outputAccessor->getDataAs(rawData)) {
        return false;
    }

    core::MemoryRange<T> outputData = rawData.getMemoryRange().viewAs<T>();

    std::vector<float> conversionData;
    if (outputAccessor->getComponentType() != gltf::ComponentType::Float) {
        if (convert(*outputAccessor, conversionData, false) == false) {
            return false;
        }
        outputData = core::makeMutableByteRange(conversionData).viewAs<T>();
    }

    if ((interpolation == gltf::AnimationInterpolation::CubicSpline && (outputData.size() / 3u < keyCount)) || (interpolation != gltf::AnimationInterpolation::CubicSpline && (outputData.size() < keyCount))) {
        core::logError("Insufficient number of keyframes for this channel");
        return false;
    }

    for (uint32_t i = 0u, o = 0u; i < keyCount; ++i) {
        // This depends on the time being stored as floats, it is currently to cumbersome to convert it properly, wait for a fix in the gltf accessor
        const float time = keys[i];
        if (interpolation != gltf::AnimationInterpolation::CubicSpline) {
            channel.addKeyframe(time, outputData[o++]);
        } else {
            const T& inTangent = outputData[o++];
            const T& value = outputData[o++];
            const T& outDataTangent = outputData[o++];
            channel.addKeyframe(time, inTangent, value, outDataTangent);
        }
    }

    animation.channels.emplace_back(std::move(channel));
    return true;
}

template <typename T>
bool addChannelToAnimation(animation::IAnimationTarget<T>& animationTarget, const gltf::AnimationChannel& gltfAnimationChannel, animation::Animation& animation)
{
    if (gltfAnimationChannel.getSampler() == nullptr) {
        return false;
    }
    auto interpolation = gltfAnimationChannel.getSampler()->getInterpolation();

    switch (interpolation) {
    case gltf::AnimationInterpolation::Step: {
        return createChannel(animation::step<T>, animationTarget, gltfAnimationChannel, animation);
    }
    case gltf::AnimationInterpolation::Linear: {
        return createChannel(animation::linear<T>, animationTarget, gltfAnimationChannel, animation);
    }
    case gltf::AnimationInterpolation::CubicSpline: {
        return createChannel(animation::cubic_spline<T>, animationTarget, gltfAnimationChannel, animation);
    }
    default:
        core::logError("Unknown animation sampler interpolation mode");
        return false;
    }
}
}

GltfRenderLayer::GltfRenderLayer(gl::GL* gl, const filesystem::Path& shaderPath)
    : m_resourceManager(gl)
    , m_cameraMapping(*this)
    , m_audioMapping(*this)
    , m_lightMapping(*this)
    , m_iblSampler(gl, shaderPath)
{
    GLint maxTextureSize;
    gl->getIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    m_max2DTextureSize = static_cast<size_t>(math::pow(maxTextureSize, 2));
    GLint maxArraySize;
    gl->getIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArraySize);
    m_maxTextureArrayLength = maxArraySize;

    // GlTextures
    m_textureMapping.createRendererObjectFromGltf = [this](std::tuple<const gltf::Image*, bool> imageAndIsLinear) -> renderer::gl::Texture* {
        auto& [gltfImage, isLinear] = imageAndIsLinear;
        if (gltfImage != nullptr && gltfImage->getEncodedBytes() != nullptr) {
            return new gl::Texture { m_resourceManager.m_gl, gl::Texture::extractTarget(gltfImage->getData()) };
        }
        return nullptr;
    };
    m_textureMapping.needsRecreation = [](renderer::gl::Texture* texture, std::tuple<const gltf::Image*, bool> imageAndIsLinear) -> bool {
        // TODO VERY BAD CODE!! REVIEW THIS!!
        auto& [gltfImage, isLinear] = imageAndIsLinear;
        gltf::Image* nonConstImg = const_cast<gltf::Image*>(gltfImage);
        return nonConstImg->hasTextureChangedForRenderer();
    };

    m_textureMapping.destroyRendererobjectFromGltf = [this](std::tuple<const gltf::Image*, bool>, renderer::gl::Texture* texture) {
        delete texture;
    };

    // GlBuffers
    m_bufferMapping.updateRendererobjectFromGltf = [](const gltf::Accessor* accessor, renderer::gl::Buffer* buffer) -> bool {
        // we need to reupload all buffers, as there is no way to check if the accessors
        // properties have changed (e.g. the component type)

        if (accessor->getBufferView() == nullptr || buffer == nullptr) {
            return false;
        }

        // ensure buffer and bufferview have same length
        const gltf::BufferView* bufferView = accessor->getBufferView();
        if (bufferView->getBuffer()->getByteLength() < bufferView->getByteLength() + bufferView->getByteOffset()) {
            return false;
        }

        core::MemoryRange<const uint8_t> range {};
        core::Slice<uint8_t> data;
        if (accessor->getSparse().has_value()) {
            if (core::Succeeded(accessor->getSparseDataAs(data))) {
                range = data.getMemoryRange();
            }
        } else {
            accessor->getData(range);
        }

        // ensure range is not bigger than buffer
        size_t rangeByteSize = range.byteSize();
        if (rangeByteSize > bufferView->getBuffer()->getByteLength()) {
            return false;
        }

        buffer->write(range);
        return true;
    };
    m_bufferMapping.createRendererObjectFromGltf = [this](const gltf::Accessor* gltfAccessor) -> renderer::gl::Buffer* {
        if (gltfAccessor->getBufferView() == nullptr) {
            return nullptr;
        }
        auto buffer = new gl::Buffer { m_resourceManager.m_gl };

        m_bufferMapping.updateRendererobjectFromGltf(gltfAccessor, buffer);

        // remove empty buffers to avoid crash errors in the draw call.
        if (buffer->getSize() == 0) {
            m_bufferMapping.destroyRendererobjectFromGltf(gltfAccessor, buffer);
            return nullptr;
        }

        return buffer;
    };
    m_bufferMapping.destroyRendererobjectFromGltf = [this](const gltf::Accessor*, renderer::gl::Buffer* buffer) {
        delete buffer;
    };

    // GlSamplerStates
    m_samplerStateMapping.updateRendererobjectFromGltf = [](const gltf::Sampler* gltfSampler, renderer::gl::SamplerState* samplerState) -> bool {
        samplerState->minFilter = renderer::gl::TextureMinFilter(gltfSampler->getMinFilter().value_or(gltf::TextureFilterType::LinearMipmapLinear));
        samplerState->magFilter = renderer::gl::TextureMagFilter(gltfSampler->getMagFilter().value_or(gltf::TextureFilterType::Linear));
        samplerState->wrapS = renderer::gl::TextureWrap(gltfSampler->getWrapSOrDefault());
        samplerState->wrapT = renderer::gl::TextureWrap(gltfSampler->getWrapTOrDefault());
        return true;
    };
    m_samplerStateMapping.createRendererObjectFromGltf = [this](const gltf::Sampler* gltfSampler) -> renderer::gl::SamplerState* {
        auto samplerState = new renderer::gl::SamplerState;
        m_samplerStateMapping.updateRendererobjectFromGltf(gltfSampler, samplerState);
        return samplerState;
    };
    m_samplerStateMapping.destroyRendererobjectFromGltf = [this](const gltf::Sampler*, renderer::gl::SamplerState* samplerState) {
        delete samplerState;
    };

    // Materials
    auto setMaterialDefaults = [this](renderer::Material* material) {
        material->samplers.clear();
        material->defines.clear();

        material->setUniform<float>("u_OcclusionStrength", 1.f);
        material->setUniform<float>("u_NormalScale", 1.f);

        // base material defaults
        material->setUniform<math::vec3>("u_EmissiveFactor", math::vec3(0.f, 0.f, 0.f));

        material->defines.define("ALPHAMODE_OPAQUE", static_cast<int>(gltf::AlphaMode::Opaque));
        material->defines.define("ALPHAMODE_MASK", static_cast<int>(gltf::AlphaMode::Mask));
        material->defines.define("ALPHAMODE_BLEND", static_cast<int>(gltf::AlphaMode::Blend));
        material->defines.define("ALPHAMODE", static_cast<int>(gltf::AlphaMode::Mask));
        material->setUniform<float>("u_AlphaCutoff", 0.0f); // default alpha mode is OPAQUE so cutoff needs to be disabled in our shader

        material->defines.define("MATERIAL_METALLICROUGHNESS");
        material->renderPass = m_resourceManager.getOpaquePass();

        // MR defaults
        material->setUniform<float>("u_MetallicFactor", 1.f);
        material->setUniform<float>("u_RoughnessFactor", 1.f);
        material->setUniform<math::vec4>("u_BaseColorFactor", math::vec4(1.f));

        // Specular Glossiness defaults
        material->setUniform<math::vec3>("u_SpecularFactor", math::vec3(1.f));
        material->setUniform<math::vec4>("u_DiffuseFactor", math::vec4(1.f));
        material->setUniform<float>("u_GlossinessFactor", 1.f);

        // clear coat defaults
        material->setUniform<float>("u_ClearcoatFactor", 0.f);
        material->setUniform<float>("u_ClearcoatRoughnessFactor", 0.f);

        // specular defaults
        material->setUniform<math::vec3>("u_KHR_materials_specular_specularColorFactor", math::vec3(1.0f));
        material->setUniform<float>("u_KHR_materials_specular_specularFactor", 1.f);

        // sheen defaults
        material->setUniform<float>("u_SheenRoughnessFactor", 0.f);
        material->setUniform<math::vec3>("u_SheenColorFactor", math::vec3(0.f));

        // IOR default
        material->setUniform<float>("u_Ior", 1.5f);

        // transmission defaults
        material->setUniform<float>("u_TransmissionFactor", 0.f);

        // volume defaults
        material->setUniform<float>("u_ThicknessFactor", 0.f);
        material->setUniform<float>("u_AttenuationDistance", 0.f);
        material->setUniform<math::vec3>("u_AttenuationColor", math::vec3(0.f));

        // iridescence defaults
        material->setUniform<float>("u_IridescenceFactor", 0.f);
        material->setUniform<float>("u_IridescenceIor", 1.3f);
        material->setUniform<float>("u_IridescenceThicknessMinimum", 100.f);
        material->setUniform<float>("u_IridescenceThicknessMaximum", 400.f);

        if (m_resourceManager.getFramebufferTexture() != nullptr && m_resourceManager.getFramebufferSamplerState() != nullptr) {
            material->defines.define("HAS_OPAQUE_FRAMEBUFFER");
            material->samplers.emplace_back("u_TransmissionFramebufferSampler", m_resourceManager.getFramebufferTexture(), m_resourceManager.getFramebufferSamplerState());
        } else {
            material->defines.undefine("HAS_OPAQUE_FRAMEBUFFER");
        }
    };

    m_materialMapping.updateRendererobjectFromGltf = [this, setMaterialDefaults, gl](const gltf::Material* gltfMaterial, renderer::Material* material) -> bool {
        setMaterialDefaults(material);
        if (gltfMaterial == nullptr) {
            return false;
        }
        material->samplers.clear();
        material->defines.clear();

        material->setUniform<float>("u_AlphaCutoff", gltfMaterial->getAlphaModeOrDefault() == gltf::AlphaMode::Mask ? static_cast<float>(gltfMaterial->getAlphaCutoffOrDefault()) : 0.f);

        // Select renderpass for transparent, transmissive, or opaque material
        if ((gltfMaterial->getAlphaModeOrDefault() == gltf::AlphaMode::Mask || gltfMaterial->getAlphaModeOrDefault() == gltf::AlphaMode::Opaque) && gltfMaterial->getTransmission().has_value() == false) {
            material->renderPass = m_resourceManager.getOpaquePass();
        }

        if (gltfMaterial->getAlphaModeOrDefault() == gltf::AlphaMode::Blend && gltfMaterial->getTransmission().has_value() == false) {
            material->renderPass = m_resourceManager.getTransparentPass();
        }

        material->defines.define("ALPHAMODE_OPAQUE", static_cast<int>(gltf::AlphaMode::Opaque));
        material->defines.define("ALPHAMODE_MASK", static_cast<int>(gltf::AlphaMode::Mask));
        material->defines.define("ALPHAMODE_BLEND", static_cast<int>(gltf::AlphaMode::Blend));
        material->defines.define("ALPHAMODE", static_cast<int>(gltfMaterial->getAlphaModeOrDefault()));

        if (gltfMaterial->getTransmission().has_value()) {
            material->renderPass = m_resourceManager.getTransmissivePass();
        }

        material->isDoubleSided = gltfMaterial->isDoubleSidedOrDefault();

        // Creates a sampler with a given name from the given texture.
        // The sampler is only created if the given texture is not null.
        // Returns true if the sampler was actually created.
        auto createSampler = [this, gltfMaterial, material, gl](const std::string& baseName, const std::optional<gltf::TextureInfo>& texture, const char* texturePointer, bool isLinear = true) -> bool {
            std::string capitalized;
            for (size_t i = 0; i < baseName.length(); ++i) {
                char c = baseName[i];
                if (i > 0 && std::isupper(c)) {
                    capitalized.push_back('_');
                }
                capitalized.push_back(std::toupper(c));
            }
            const std::string samplerEnabledDefine = "HAS_" + capitalized + "_MAP";

            if (texture.has_value() == false) {
                return false;
            }

            const gltf::TextureInfo& info = texture.value();
            const gltf::Texture* gltfTexture = info.getTexture();

            if (gltfTexture == nullptr) {
                return false;
            }

            auto texIt = m_textureMapping.map.end();

            const auto& oBasisU = gltfTexture->getBasisU();
            if (oBasisU.has_value()) {
                texIt = m_textureMapping.map.find({ oBasisU.value().getSource(), isLinear });
            }

            const auto& oWebP = gltfTexture->getWebP();
            if (oWebP.has_value()) {
                texIt = m_textureMapping.map.find({ oWebP.value().getSource(), isLinear });
            }

            if (texIt == m_textureMapping.map.end() || texIt->second == nullptr) {
                // fall back to standard texture source
                texIt = m_textureMapping.map.find({ gltfTexture->getSource(), isLinear });
                if (texIt == m_textureMapping.map.end() || texIt->second == nullptr) {
                    return false;
                }
            }
            renderer::gl::Texture* glTexture = texIt->second;
            const gltf::Image* gltfImage = std::get<const gltf::Image*>(texIt->first);

            const renderer::gl::SamplerState* samplerState = nullptr;
            auto samplerIt = m_samplerStateMapping.map.find(gltfTexture->getSampler());
            if (samplerIt != m_samplerStateMapping.map.end()) {
                samplerState = samplerIt->second;
            }

            if (glTexture->currentMapID != gltfImage->encodedMapID) {
                glTexture->currentMapID = gltfImage->encodedMapID;
                glTexture->write(gltfImage->getData(), isLinear);
                if (samplerState != nullptr) {
                    if (samplerState->minFilter == gl::TextureMinFilter::NearestMipmapNearest
                        || samplerState->minFilter == gl::TextureMinFilter::NearestMipmapLinear
                        || samplerState->minFilter == gl::TextureMinFilter::LinearMipmapNearest
                        || samplerState->minFilter == gl::TextureMinFilter::LinearMipmapLinear) {
                        gl->generateMipmap(static_cast<GLenum>(glTexture->target));
                    }
                }
            }

            material->samplers.emplace_back("u_" + baseName + "Sampler", glTexture, samplerState);
            material->setUniform<uint32_t>("u_" + baseName + "UVSet", info.getEffectiveTexCoord());
            if (info.getTextureTransform()) {
                std::string uniformName = "u_" + baseName + "UVTransform";
                material->setUniform<math::mat3>(uniformName, info.calculateTransformMatrix());

                auto transform = std::make_shared<gltf::TextureTransformExtension>(*info.getTextureTransform());

                auto setupAnimatedTextureTransform = [this, gltfMaterial, material, uniformName, transform](std::string propertyPrefix) {
                    m_animationPointerTargets[{ gltfMaterial, propertyPrefix + "/extensions/KHR_texture_transform/offset" }].callback = KHRAnimationPointerTarget::Callback<glm::vec2> {
                        [material, uniformName, transform](const glm::vec2& value) -> void {
                            transform->setOffset(value);
                            material->setUniform<glm::mat3>(uniformName, transform->calculateTransformMatrix());
                        }
                    };
                    m_animationPointerTargets[{ gltfMaterial, propertyPrefix + "/extensions/KHR_texture_transform/scale" }].callback = KHRAnimationPointerTarget::Callback<glm::vec2> {
                        [material, uniformName, transform](const glm::vec2& value) -> void {
                            transform->setScale(value);
                            material->setUniform<glm::mat3>(uniformName, transform->calculateTransformMatrix());
                        }
                    };
                    m_animationPointerTargets[{ gltfMaterial, propertyPrefix + "/extensions/KHR_texture_transform/rotation" }].callback = KHRAnimationPointerTarget::Callback<float> {
                        [material, uniformName, transform](const float& value) -> void {
                            transform->setRotation(value);
                            material->setUniform<glm::mat3>(uniformName, transform->calculateTransformMatrix());
                        }
                    };
                };

                if (texturePointer != nullptr) {
                    setupAnimatedTextureTransform(texturePointer);
                }

                std::string upperBaseName = baseName;
                std::transform(baseName.begin(), baseName.end(), upperBaseName.begin(), ::toupper);
                material->defines.define("HAS_" + upperBaseName + "_UV_TRANSFORM");
            }

            material->defines.define(samplerEnabledDefine);

            return true;
        };

        createSampler("Normal", gltfMaterial->getNormalTextureInfo(), "normalTexture");
        createSampler("Emissive", gltfMaterial->getEmissiveTextureInfo(), "emissiveTexture", false);
        createSampler("Occlusion", gltfMaterial->getOcclusionTextureInfo(), "occlusionTexture");

        material->setUniform<math::vec3>("u_EmissiveFactor", gltfMaterial->getEmissiveFactorOrDefault());
        material->setUniform<float>("u_NormalScale", gltfMaterial->getNormalTextureInfo().has_value() ? static_cast<float>(gltfMaterial->getNormalTextureInfo()->getScale()) : 1.f);
        material->setUniform<float>("u_OcclusionStrength", gltfMaterial->getOcclusionTextureInfo().has_value() ? static_cast<float>(gltfMaterial->getOcclusionTextureInfo()->getScale()) : 1.f);

        setUniformAsAnimationPointerTarget<float>(gltfMaterial, "alphaCutoff", material, "u_AlphaCutoff");
        setUniformAsAnimationPointerTarget<glm::vec3>(gltfMaterial, "emissiveFactor", material, "u_EmissiveFactor");
        setUniformAsAnimationPointerTarget<float>(gltfMaterial, "normalTexture/scale", material, "u_NormalScale");
        setUniformAsAnimationPointerTarget<float>(gltfMaterial, "occlusionTexture/strength", material, "u_OcclusionStrength");

        const auto& metallicRoughness = gltfMaterial->getPbrMetallicRoughness();
        if (metallicRoughness.has_value()) {
            createSampler("BaseColor", metallicRoughness->getBaseColorTextureInfo(), "pbrMetallicRoughness/baseColorTexture", false);
            createSampler("MetallicRoughness", metallicRoughness->getMetallicRoughnessTextureInfo(), "pbrMetallicRoughness/metallicRoughnessTexture");
            material->setUniform<float>("u_MetallicFactor", static_cast<float>(metallicRoughness->getMetallicFactorOrDefault()));
            material->setUniform<float>("u_RoughnessFactor", static_cast<float>(metallicRoughness->getRoughnessFactorOrDefault()));
            material->setUniform<math::vec4>("u_BaseColorFactor", metallicRoughness->getBaseColorFactorOrDefault());

            setUniformAsAnimationPointerTarget<glm::vec4>(gltfMaterial, "pbrMetallicRoughness/baseColorFactor", material, "u_BaseColorFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "pbrMetallicRoughness/metallicFactor", material, "u_MetallicFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "pbrMetallicRoughness/roughnessFactor", material, "u_RoughnessFactor");
        }

        const auto& specularGlossiness = gltfMaterial->getPbrSpecularGlossiness();
        if (specularGlossiness.has_value()) {
            createSampler("SpecularGlossiness", specularGlossiness->getSpecularGlossinessTextureInfo(), nullptr, false);
            createSampler("Diffuse", specularGlossiness->getDiffuseTextureInfo(), nullptr, false);
            material->setUniform<float>("u_GlossinessFactor", static_cast<float>(specularGlossiness->getGlossinessFactorOrDefault()));
            material->setUniform<math::vec3>("u_SpecularFactor", specularGlossiness->getSpecularFactorOrDefault());
            material->setUniform<math::vec4>("u_DiffuseFactor", specularGlossiness->getDiffuseFactorOrDefault());
        }

        const auto& clearCoat = gltfMaterial->getClearcoat();
        if (clearCoat.has_value()) {
            createSampler("Clearcoat", clearCoat->getClearcoatTexture(), "extensions/KHR_materials_clearcoat/clearcoatTexture");
            createSampler("ClearcoatRoughness", clearCoat->getClearcoatRoughnessTexture(), "extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture");
            createSampler("ClearcoatNormal", clearCoat->getClearcoatNormalTexture(), "extensions/KHR_materials_clearcoat/clearcoatNormalTexture");
            material->setUniform<float>("u_ClearcoatNormalScale", clearCoat->getClearcoatNormalTexture().has_value() ? static_cast<float>(clearCoat->getClearcoatNormalTexture()->getScale()) : 1.f);
            material->setUniform<float>("u_ClearcoatFactor", static_cast<float>(clearCoat->getClearcoatFactorOrDefault()));
            material->setUniform<float>("u_ClearcoatRoughnessFactor", static_cast<float>(clearCoat->getClearcoatRoughnessFactorOrDefault()));

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatFactor", material, "u_ClearcoatFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatRoughnessFactor", material, "u_ClearcoatRoughnessFactor");
        }

        const auto& specular = gltfMaterial->getSpecular();
        if (specular.has_value()) {
            material->setUniform<math::vec3>("u_KHR_materials_specular_specularColorFactor", specular->getSpecularColorFactorOrDefault());
            material->setUniform<float>("u_KHR_materials_specular_specularFactor", static_cast<float>(specular->getSpecularFactorOrDefault()));
            createSampler("Specular", specular->getSpecularTextureInfo(), "extensions/KHR_materials_specular/specularTexture");
            createSampler("SpecularColor", specular->getSpecularColorTextureInfo(), "extensions/KHR_materials_specular/specularColorTexture", false);

            setUniformAsAnimationPointerTarget<glm::vec3>(gltfMaterial, "extensions/KHR_materials_specular/specularColorFactor", material, "u_KHR_materials_specular_specularColorFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_specular/specularFactor", material, "u_KHR_materials_specular_specularFactor");
        }

        const auto& emissiveStrength = gltfMaterial->getEmissiveStrength();
        if (emissiveStrength.has_value()) {
            material->defines.define("MATERIAL_EMISSIVE_STRENGTH");
            material->setUniform<float>("u_EmissiveStrength", emissiveStrength->getEmissiveStrengthOrDefault());

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_emissive_strength/emissiveStrength", material, "u_EmissiveStrength");
        }

        const auto& sheen = gltfMaterial->getSheen();
        if (sheen.has_value()) {
            createSampler("SheenColor", sheen->getSheenColorTextureInfo(), "extensions/KHR_materials_sheen/sheenColorTexture", false);
            createSampler("SheenRoughness", sheen->getSheenRoughnessTextureInfo(), "extensions/KHR_materials_sheen/sheenRoughnessTexture");
            material->setUniform<float>("u_SheenRoughnessFactor", static_cast<float>(sheen->getSheenRoughnessFactorOrDefault()));
            material->setUniform<glm::vec3>("u_SheenColorFactor", sheen->getSheenColorFactorOrDefault());

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_sheen/sheenRoughnessFactor", material, "u_SheenRoughnessFactor");
            setUniformAsAnimationPointerTarget<glm::vec3>(gltfMaterial, "extensions/KHR_materials_sheen/sheenColorFactor", material, "u_SheenColorFactor");
        }

        const auto& ior = gltfMaterial->getIor();
        if (ior.has_value() && renderSettings.enableIOR) {
            material->setUniform<float>("u_Ior", static_cast<float>(ior->getIorOrDefault()));

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_ior/ior", material, "u_Ior");
        }

        const auto& transmission = gltfMaterial->getTransmission();
        if (transmission.has_value()) {
            createSampler("Transmission", transmission->getTransmissionTextureInfo(), "extensions/KHR_materials_transmission/transmissionTexture");
            material->setUniform<float>("u_TransmissionFactor", static_cast<float>(transmission->getTransmissionFactorOrDefault()));

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_transmission/transmissionFactor", material, "u_TransmissionFactor");
        }

        const auto& volume = gltfMaterial->getVolume();
        if (volume.has_value()) {
            createSampler("Thickness", volume->getThicknessTextureInfo(), "extensions/KHR_materials_volume/thicknessTexture");
            material->setUniform<float>("u_ThicknessFactor", static_cast<float>(volume->getThicknessFactorOrDefault()));
            material->setUniform<float>("u_AttenuationDistance", static_cast<float>(volume->getAttenuationDistance().value_or(0)));
            material->setUniform<math::vec3>("u_AttenuationColor", volume->getAttenuationColorOrDefault());

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_volume/thicknessFactor", material, "u_ThicknessFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_volume/attenuationDistance", material, "u_AttenuationDistance");
            setUniformAsAnimationPointerTarget<glm::vec3>(gltfMaterial, "extensions/KHR_materials_volume/attenuationColor", material, "u_AttenuationColor");
        }

        const auto& iridescence = gltfMaterial->getIridescence();
        if (iridescence.has_value()) {
            createSampler("Iridescence", iridescence->getIridescenceTextureInfo(), "extensions/KHR_materials_iridescence/iridescenceTexture");
            createSampler("IridescenceThickness", iridescence->getIridescenceThicknessTextureInfo(), "extensions/KHR_materials_iridescence/iridescenceThicknessTexture");
            material->setUniform<float>("u_IridescenceFactor", static_cast<float>(iridescence->getIridescenceFactorOrDefault()));
            material->setUniform<float>("u_IridescenceIor", static_cast<float>(iridescence->getIridescenceIorOrDefault()));
            material->setUniform<float>("u_IridescenceThicknessMinimum", static_cast<float>(iridescence->getIridescenceThicknessMinimumOrDefault()));
            material->setUniform<float>("u_IridescenceThicknessMaximum", static_cast<float>(iridescence->getIridescenceThicknessMaximumOrDefault()));

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceFactor", material, "u_IridescenceFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceIor", material, "u_IridescenceIor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessMaximum", material, "u_IridescenceThicknessMaximum");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessMinimum", material, "u_IridescenceThicknessMinimum");
        }

        if (m_resourceManager.getFramebufferTexture() != nullptr && m_resourceManager.getFramebufferSamplerState() != nullptr) {
            material->defines.define("HAS_OPAQUE_FRAMEBUFFER");
            material->samplers.emplace_back("u_TransmissionFramebufferSampler", m_resourceManager.getFramebufferTexture(), m_resourceManager.getFramebufferSamplerState());

            math::ivec2 framebufferSize;
            gl->bindTexture(GL_TEXTURE_2D, m_resourceManager.getFramebufferTexture()->handle);
            gl->getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &framebufferSize.x);
            gl->getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &framebufferSize.y);

            material->setUniform<math::ivec2>("u_TransmissionFramebufferSize", framebufferSize);

        } else {
            material->defines.undefine("HAS_OPAQUE_FRAMEBUFFER");
        }

        material->defines.defineIf(true, "MATERIAL_METALLICROUGHNESS"); // Always set as fallback
        material->defines.defineIf(specularGlossiness.has_value(), "MATERIAL_SPECULARGLOSSINESS");
        material->defines.defineIf(gltfMaterial->isUnlit(), "MATERIAL_UNLIT");
        material->defines.defineIf(clearCoat.has_value(), "MATERIAL_CLEARCOAT");
        material->defines.defineIf(specular.has_value(), "MATERIAL_SPECULAR");
        material->defines.defineIf(sheen.has_value(), "MATERIAL_SHEEN");
        material->defines.defineIf(ior.has_value(), "MATERIAL_IOR");
        material->defines.defineIf(transmission.has_value(), "MATERIAL_TRANSMISSION");
        material->defines.defineIf(volume.has_value(), "MATERIAL_VOLUME");
        material->defines.defineIf(iridescence.has_value(), "MATERIAL_IRIDESCENCE");

        return true;
    };

    m_materialMapping.createRendererObjectFromGltf = [this, setMaterialDefaults](const gltf::Material* gltfMaterial) -> renderer::Material* {
        auto material = new renderer::Material { m_resourceManager.m_gl };
        material->fragmentShader = "pbr.frag";

        setMaterialDefaults(material);

        m_materialMapping.updateRendererobjectFromGltf(gltfMaterial, material);

        return material;
    };

    m_materialMapping.destroyRendererobjectFromGltf = [this](const gltf::Material* gltfMaterial, renderer::Material* material) {
        delete material;

        m_animationPointerTargets.erase({ gltfMaterial, "alphaCutoff" });
        m_animationPointerTargets.erase({ gltfMaterial, "emissiveFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "normalTexture/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "occlusionTexture/strength" });

        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/baseColorFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/metallicFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/roughnessFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/baseColorTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/baseColorTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/baseColorTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/metallicRoughnessTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/metallicRoughnessTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/metallicRoughnessTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatRoughnessFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatNormalTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatNormalTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatNormalTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularColorFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularColorTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularColorTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularColorTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_emissive_strength/emissiveStrength" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenRoughnessFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenColorFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenColorTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenColorTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenColorTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenRoughnessTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenRoughnessTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenRoughnessTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_ior/ior" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_transmission/transmissionFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_transmission/transmissionTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_transmission/transmissionTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_transmission/transmissionTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/thicknessFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/attenuationDistance" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/attenuationColor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/thicknessTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/thicknessTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/thicknessTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceIor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessMaximum" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessMinimum" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessTexture/extensions/KHR_texture_transform/scale" });
    };
    m_materialMapping.needsRecreation = [](renderer::Material* material, const gltf::Material* gltfMaterial) -> bool {
        return false;
    };

    // Mesh
    m_meshMapping.updateRendererobjectFromGltf = [this](std::tuple<const gltf::Node*, const gltf::Mesh*, const gltf::Primitive*> gltfTuple, renderer::Mesh* mesh) -> bool {
        auto [gltfNode, gltfMesh, gltfPrimitive] = gltfTuple;

        const gltf::Accessor* indexAccessor = gltfPrimitive->getIndicesAccessor();
        mesh->primitiveTopology = renderer::gl::PrimitiveTopology(gltfPrimitive->getPrimitiveMode());

        if (indexAccessor != nullptr) {
            mesh->indexFormat = renderer::gl::PrimitiveType(indexAccessor->getComponentType());
            mesh->indexCount = indexAccessor->getCount();
            mesh->indexByteOffset = 0u; // offset is already resolved in accessor

            auto bufferIt = m_bufferMapping.map.find(indexAccessor);
            if (bufferIt != m_bufferMapping.map.end()) {
                mesh->indexBuffer = bufferIt->second;
            } else {
                core::logError("No gl index buffer found for gltf primitive");
                return false;
            }
        }

        auto nodeIt = m_transformHierarchyMapping.map.find(gltfNode);
        if (nodeIt != m_transformHierarchyMapping.map.end()) {
            nodeIt->second->addMesh(mesh);
        }

        uint32_t vertexCount = 0u;
        auto addAttribute = [&](const gltf::Accessor* accessor, const std::string& name) {
            if (accessor == nullptr)
                return;

            if (mesh->attributes.size() >= 16) {
                // more attributes than GL_MAX_VERTEX_ATTRIBUTES (on most platforms)
                return;
            }

            // check if accessor has bufferview
            if (accessor->getBufferView() == nullptr) {
                return;
            }

            if (vertexCount == 0u) {
                vertexCount = accessor->getCount();
            } else if (vertexCount != accessor->getCount()) {
                core::logError("Inconsistent attribute element count found for attribute %s", name.c_str());
                return;
            }

            renderer::gl::Attribute attribute("a_" + core::toLower(name));

            auto bufferIt = m_bufferMapping.map.find(accessor);
            if (bufferIt != m_bufferMapping.map.end()) {
                if (bufferIt->second != nullptr) {
                    attribute.buffer = bufferIt->second;
                    // skip uninitialized buffers (dont add them to the mesh) (can happen with broken gltfs)
                    if (attribute.buffer->getSize() == 0) {
                        return;
                    }
                }
            } else {
                core::logError("No gl buffer found for gltf attribute %s", name.c_str());
                return;
            }

            std::string define = "HAS_" + core::toUpper(name) + "_";
            switch (accessor->getType()) {
            case gltf::AttributeType::Scalar:
                define += "FLOAT";
                break;
            case gltf::AttributeType::Vec2:
                define += "VEC2";
                break;
            case gltf::AttributeType::Vec3:
                define += "VEC3";
                break;
            case gltf::AttributeType::Vec4:
                define += "VEC4";
                break;
            case gltf::AttributeType::Mat2:
                define += "MAT2";
                break;
            case gltf::AttributeType::Mat3:
                define += "MAT3";
                break;
            case gltf::AttributeType::Mat4:
                define += "MAT4";
                break;
            default:
                break;
            }
            mesh->defines.define(define);

            const gltf::BufferView* bufferView = accessor->getBufferView();

            attribute.offset = 0u;
            attribute.stride = bufferView ? bufferView->getByteStride().value_or(0u) : 0u;
            attribute.normalized = accessor->isNormalized() ? GL_TRUE : GL_FALSE;
            attribute.type = static_cast<gl::ComponentType>(accessor->getComponentType());

            switch (accessor->getType()) {
            case gltf::AttributeType::Scalar:
                attribute.dimension = 1;
                break;
            case gltf::AttributeType::Vec2:
                attribute.dimension = 2;
                break;
            case gltf::AttributeType::Vec3:
                attribute.dimension = 3;
                break;
            case gltf::AttributeType::Vec4:
                attribute.dimension = 4;
                break;
            default:
                break;
            }

            mesh->attributes.emplace_back(attribute);
        };

        mesh->attributes.clear();
        mesh->defines.clear();
        for (const gltf::Attribute& gltfAttribute : gltfPrimitive->getAttributes()) {
            addAttribute(gltfAttribute.getAccessor(), gltfAttribute.getName());
        }

        mesh->vertexCount = vertexCount;

        if (gltfPrimitive->getTargets().size() > 0) {
            size_t attributeCount = 0;
            std::unordered_map<std::string, gltf::Accessor*> attributes;
            mesh->defines.define("NUM_VERTICIES", vertexCount);
            mesh->defines.define("HAS_MORPH_TARGETS", 1);

            for (auto& target : gltfPrimitive->getTargets()) {
                attributeCount += target.accessorCount();
                attributes.merge(target.getAccessorMapping());
            }

            size_t maxTargetAttribute = attributes.size();
            size_t targetCount = gltfPrimitive->getTargets().size();
            if (targetCount * attributes.size() > m_maxTextureArrayLength) {
                targetCount = m_maxTextureArrayLength / attributes.size();
            }

            size_t attributeOffset = 0;
            std::unordered_map<std::string, size_t> attributeOffsets;
            for (auto& [attribute, _] : attributes) {
                mesh->defines.define("HAS_MORPH_TARGET_" + attribute, 1);
                mesh->defines.define("MORPH_TARGET_" + attribute + "_OFFSET", static_cast<int>(attributeOffset));
                attributeOffsets[attribute] = attributeOffset;
                attributeOffset += targetCount;
            }

            // Create texture
            if (attributeCount != mesh->morphTargetAttributeCount) {
                mesh->morphTargetAttributeCount = attributeCount;
                if (vertexCount <= m_max2DTextureSize) {
                    if (gltfPrimitive->getTargets().size() * attributeOffsets.size() > m_maxTextureArrayLength) {
                        core::logWarning("Morph targets exceed texture size limit. Only %u of %u are used", targetCount, gltfPrimitive->getTargets().size());
                    }
                    const uint32_t width = static_cast<uint32_t>(math::ceil(math::sqrt(vertexCount)));
                    const size_t singleTextureSize = static_cast<size_t>(math::pow(width, 2) * 4);
                    const size_t dataSize = singleTextureSize * targetCount * attributeOffsets.size();
                    std::vector<float> textureData(dataSize, 0.0f);
                    for (int i = 0; i < targetCount; ++i) {
                        const auto& target = gltfPrimitive->getTargets().at(i);
                        for (auto& [accessorName, offsetRef] : attributeOffsets) {
                            auto targetAccessorMap = target.getAccessorMapping();
                            if (targetAccessorMap.find(accessorName) != targetAccessorMap.end()) {
                                auto accessor = targetAccessorMap.at(accessorName);
                                size_t offset = offsetRef * singleTextureSize;
                                core::MemoryRange<float> accessorData;

                                core::Slice<float> accessorSliceData;
                                if (accessor->getSparse().has_value()) {
                                    if (!accessor->getSparseDataAs(accessorSliceData)) {
                                        core::logWarning("Could not generate morph target texture");
                                        return false;
                                    }
                                } else if (!accessor->getDataAs(accessorSliceData)) {
                                    core::logWarning("Could not generate morph target texture");
                                    return false;
                                }

                                std::vector<float> floatData;
                                if (accessor->getComponentType() != gltf::ComponentType::Float) {
                                    if (convert(*accessor, floatData, true) == false) {
                                        core::logWarning("Could not generate morph target texture");
                                        return false;
                                    }
                                    accessorData = core::makeMutableByteRange(floatData).viewAs<float>();

                                } else {
                                    accessorData = accessorSliceData.getMemoryRange();
                                }

                                const auto accessorType = accessor->getType();
                                switch (accessorType) {
                                case gltf::AttributeType::Vec2:
                                case gltf::AttributeType::Vec3: {
                                    // Add padding to fit vec2/vec3 into rgba
                                    size_t paddingOffset = 0;
                                    size_t accessorOffset = 0;
                                    const size_t componentCount = gltf::Accessor::getComponentCountFrom(accessorType);
                                    for (size_t j = 0; j < accessor->getCount(); ++j) {
                                        std::copy(accessorData.begin() + accessorOffset, accessorData.begin() + accessorOffset + componentCount, textureData.begin() + offset + paddingOffset);
                                        paddingOffset += 4;
                                        accessorOffset += componentCount;
                                    }
                                    break;
                                }
                                case gltf::AttributeType::Vec4:
                                    std::copy(accessorData.begin(), accessorData.end(), textureData.begin() + offset);
                                    break;
                                default:
                                    core::logWarning("Unsupported attribute type for morph targets");
                                    break;
                                }
                            }
                            ++offsetRef;
                        }
                    }
                    mesh->setMorphTargetTexture(width, textureData, targetCount * attributeOffsets.size());
                } else {
                    core::logWarning("Mesh of Morph targets too big. Cannot apply morphing.");
                }
            }
        }
        auto gltfWeightCount = static_cast<int>(std::max(gltfNode->weights ? gltfNode->weights->size() : 0, gltfMesh->weights ? gltfMesh->weights->size() : 0));
        if (gltfWeightCount > 128) {
            core::logWarning("This asset uses a high number of weights: %d", gltfWeightCount);
        }
        gltfWeightCount = std::max(gltfWeightCount, 1); // shader requires at least one item
        mesh->defines.define("WEIGHT_COUNT", gltfWeightCount);

        m_animationPointerTargets[{ gltfMesh, "weights" }].callback = KHRAnimationPointerTarget::Callback<std::vector<float>> {
            [mesh](const std::vector<float>& value) -> void {
                mesh->updateWeights(value);
            }
        };

        mesh->polygonMode = gl::PolygonMode::Fill;
        mesh->defines.define("DEBUG", static_cast<int>(RenderMode::None));

        if (nodeIt != m_transformHierarchyMapping.map.end()) {
            // from the spec: If the determinant of the transform is a negative value, the winding order of the mesh triangle faces should be reversed. This supports negative scales for mirroring geometry.
            const float det = math::determinant(nodeIt->second->getWorldTransform());
            if (det < 0.f) {
                mesh->frontFace = gl::Winding::Clockwise;
            } else {
                mesh->frontFace = gl::Winding::CounterClockwise;
            }
        }

        mesh->bumpGeneration();

        return true;
    };

    m_meshMapping.createRendererObjectFromGltf = [this](std::tuple<const gltf::Node*, const gltf::Mesh*, const gltf::Primitive*> gltfTuple) -> renderer::Mesh* {
        auto mesh = new Mesh { m_resourceManager.m_gl };
        mesh->vertexShader = "primitive.vert";
        m_meshMapping.updateRendererobjectFromGltf(gltfTuple, mesh);
        return mesh;
    };
    m_meshMapping.destroyRendererobjectFromGltf = [this](std::tuple<const gltf::Node*, const gltf::Mesh*, const gltf::Primitive*> gltfTuple, renderer::Mesh* mesh) {
        auto [gltfNode, gltfMesh, gltfPrimitive] = gltfTuple;

        auto nodeIt = m_transformHierarchyMapping.map.find(gltfNode);
        if (nodeIt != m_transformHierarchyMapping.map.end()) {
            nodeIt->second->removeMesh(mesh);
        }

        // remove mesh from all skins that use it
        for (const auto& [_, skin] : m_skinMapping.map) {
            if (skin != nullptr) {
                skin->removeMesh(mesh);
            }
        }

        m_animationPointerTargets.erase({ gltfMesh, "weights" });

        delete mesh;
    };
    m_meshMapping.needsRecreation = [this](renderer::Mesh*, std::tuple<const gltf::Node*, const gltf::Mesh*, const gltf::Primitive*>) -> bool {
        return false;
    };

    // Transform hierarchy
    m_transformHierarchyMapping.updateRendererobjectFromGltf = [this](const gltf::Node* gltfNode, renderer::TransformHierarchyNode* node) -> bool {
        node->translationTarget.update(gltfNode->getTranslationOrDefault());
        node->rotationTarget.update(gltfNode->getRotationOrDefault());
        node->scaleTarget.update(gltfNode->getScaleOrDefault());

        std::vector<float> weights;
        if (gltfNode->weights && !gltfNode->weights->empty()) {
            for (double w : *gltfNode->weights) {
                weights.emplace_back(static_cast<float>(w));
            }
            node->weightTarget.update(weights);
        } else {
            if (gltfNode->getMesh() != nullptr) {
                if (gltfNode->getMesh()->weights) {
                    for (double w : *gltfNode->getMesh()->weights) {
                        weights.emplace_back(static_cast<float>(w));
                    }
                }
                node->weightTarget.update(weights);
            }
        }

        m_animationPointerTargets[{ gltfNode, "translation" }].callback = KHRAnimationPointerTarget::Callback<glm::vec3> {
            [node](const glm::vec3& value) -> void {
                node->translationTarget.update(value);
            }
        };

        m_animationPointerTargets[{ gltfNode, "rotation" }].callback = KHRAnimationPointerTarget::Callback<glm::quat> {
            [node](const glm::quat& value) -> void {
                node->rotationTarget.update(math::dquat(value));
            }
        };

        m_animationPointerTargets[{ gltfNode, "scale" }].callback = KHRAnimationPointerTarget::Callback<glm::vec3> {
            [node](const glm::vec3& value) -> void {
                node->scaleTarget.update(value);
            }
        };

        m_animationPointerTargets[{ gltfNode, "weights" }].callback = KHRAnimationPointerTarget::Callback<std::vector<float>> {
            [node](const std::vector<float>& value) -> void {
                node->weightTarget.update(value);
            }
        };

        return true;
    };
    m_transformHierarchyMapping.createRendererObjectFromGltf = [this](const gltf::Node* gltfNode) -> renderer::TransformHierarchyNode* {
        recalculateBones = true;
        std::vector<float> weights;
        if (!gltfNode->weights && gltfNode->getMesh() != nullptr) {
            // Use mesh weights
            if (gltfNode->getMesh()->weights) {
                for (double w : *gltfNode->getMesh()->weights) {
                    weights.emplace_back(static_cast<float>(w));
                }
            }
        } else if (gltfNode->weights) {
            // Use node weights
            for (double w : *gltfNode->weights) {
                weights.emplace_back(static_cast<float>(w));
            }
        }

        auto node = m_resourceManager.m_transformHierarchy.createNode(gltfNode->getTranslationOrDefault(), gltfNode->getRotationOrDefault(), gltfNode->getScaleOrDefault(), weights);
        m_transformHierarchyMapping.updateRendererobjectFromGltf(gltfNode, node);
        return node;
    };
    m_transformHierarchyMapping.destroyRendererobjectFromGltf = [this](const gltf::Node* gltfNode, renderer::TransformHierarchyNode* node) {
        m_resourceManager.m_transformHierarchy.eraseNode(node);

        m_animationPointerTargets.erase({ gltfNode, "translation" });
        m_animationPointerTargets.erase({ gltfNode, "rotation" });
        m_animationPointerTargets.erase({ gltfNode, "scale" });
        m_animationPointerTargets.erase({ gltfNode, "weights" });
        recalculateBones = true;
    };

    // Skin
    m_skinMapping.updateRendererobjectFromGltf = [this](std::tuple<const gltf::Node*, const gltf::Skin*> gltfTuple, renderer::Skin* skin) -> bool {
        if (skin == nullptr) {
            return true;
        }

        auto [gltfNode, gltfSkin] = gltfTuple;
        for (const auto& primitive : gltfNode->getMesh()->getPrimitives()) {
            if (auto meshIt = m_meshMapping.map.find(std::make_tuple(gltfNode, gltfNode->getMesh(), &primitive));
                meshIt != m_meshMapping.map.end()) {
                auto& [_, mesh] = *meshIt;
                mesh->defines.defineIf(renderSettings.enableSkinning, "USE_SKINNING");
                skin->addMesh(mesh);
            }
        }
        recalculateBones = true;
        return true;
    };

    m_skinMapping.createRendererObjectFromGltf = [this, gl](std::tuple<const gltf::Node*, const gltf::Skin*> gltfTuple) -> renderer::Skin* {
        auto [gltfNode, gltfSkin] = gltfTuple;

        // We need 8 pixels for each joint (2 x 4x4 matrix)
        const size_t maxJointCount = m_max2DTextureSize / 8;
        if (gltfSkin->getJoints().size() > maxJointCount) {
            core::logError("Skin with %u joints exceeds number of supported joints %u", static_cast<uint32_t>(gltfSkin->getJoints().size()), maxJointCount);
            return nullptr;
        }

        auto& skin = m_skins.emplace_back(gl);
        for (auto gltfJoint : gltfSkin->getJoints()) {
            auto jointIt = m_transformHierarchyMapping.map.find(gltfJoint);
            if (jointIt != m_transformHierarchyMapping.map.end()) {
                skin.addJoint(jointIt->second);
            }
        }
        if (gltfSkin->getInverseBindMatricesAccessor() != nullptr) {
            auto accessor = gltfSkin->getInverseBindMatricesAccessor();

            core::Slice<math::mat4x4> IBMdata;
            if (accessor->getSparse().has_value()) {
                accessor->getSparseDataAs(IBMdata);
            } else {
                accessor->getDataAs(IBMdata);
            }

            for (uint32_t i = 0u; i < IBMdata.size(); ++i) {
                skin.addIBM(IBMdata[i]);
            }
        } else {
            const math::mat4x4 identity(1.f);
            for (uint32_t i = 0u; i < gltfSkin->getJoints().size(); ++i) {
                skin.addIBM(identity);
            }
        }

        // Sets root of skin to cancel transform during joint calculation
        auto rootIt = m_transformHierarchyMapping.map.find(gltfNode);
        if (rootIt != m_transformHierarchyMapping.map.end()) {
            skin.setRoot(rootIt->second);
        }

        m_skinMapping.updateRendererobjectFromGltf(gltfTuple, &skin);
        recalculateBones = true;
        return &skin;
    };
    m_skinMapping.destroyRendererobjectFromGltf = [this](std::tuple<const gltf::Node*, const gltf::Skin*> gltfTuple, renderer::Skin* skin) {
        // Sets uniforms in meshes of skin, if they still exist
        auto iter = std::find_if(m_skins.begin(), m_skins.end(), [skin](auto& x) { return &x == skin; });
        if (iter != m_skins.end()) {
            for (Mesh* mesh : iter->getMeshes()) {
                // the mesh can already be deleted (updateMeshes is called before updateSkin in update)
                if (mesh != nullptr) {
                    mesh->defines.undefine("USE_SKINNING");
                }
            }
            m_skins.erase(iter);
        }
        recalculateBones = true;
    };

    // Animations
    m_animationMapping.createRendererObjectFromGltf = [this](const gltf::Animation* gltfAnimation) -> animation::Animation* {
        float animationTime = 0.f;

        if (gltfAnimation->isPlaying()) {
            for (const gltf::AnimationSampler& gltfSampler : gltfAnimation->getSamplers()) {
                auto accessor = gltfSampler.getInputAccessor();

                if (accessor->getComponentType() != gltf::ComponentType::Float) {
                    core::log(core::LogLevel::Error, "Animation time is not provided in floats. This is currently not supported");
                    continue;
                }

                core::Slice<float> rawData;
                if (accessor->getSparse().has_value()) {
                    accessor->getSparseDataAs(rawData);
                } else {
                    accessor->getDataAs(rawData);
                }

                if (rawData.empty() == false) {
                    float time = rawData.back();
                    if (time > animationTime) {
                        animationTime = time;
                    }
                }
            }
        }

        animation::Animation animation(animationTime);

        animation.isPlaying = gltfAnimation->isPlaying();

        if (gltfAnimation->isPlaying()) {
            for (const gltf::AnimationChannel& gltfChannel : gltfAnimation->getChannels()) {
                auto path = gltfChannel.getTarget().getPath();

                if (path == gltf::AnimationPath::Pointer && gltfChannel.getTarget().m_animationPointer.has_value()) {
                    auto animationPointerTuple = gltfChannel.getTarget().m_animationPointer.value();
                    if (m_animationPointerTargets.count(animationPointerTuple) == 0) {
                        continue;
                    }

                    KHRAnimationPointerTarget& animationPointerTarget = m_animationPointerTargets[animationPointerTuple];

                    if (std::holds_alternative<KHRAnimationPointerTarget::Callback<float>>(animationPointerTarget.callback)) {
                        addChannelToAnimation(
                            static_cast<animation::IAnimationTarget<float>&>(animationPointerTarget),
                            gltfChannel, animation);
                    } else if (std::holds_alternative<KHRAnimationPointerTarget::Callback<glm::vec2>>(animationPointerTarget.callback)) {
                        addChannelToAnimation(
                            static_cast<animation::IAnimationTarget<glm::vec2>&>(animationPointerTarget),
                            gltfChannel, animation);
                    } else if (std::holds_alternative<KHRAnimationPointerTarget::Callback<glm::vec3>>(animationPointerTarget.callback)) {
                        addChannelToAnimation(
                            static_cast<animation::IAnimationTarget<glm::vec3>&>(animationPointerTarget),
                            gltfChannel, animation);
                    } else if (std::holds_alternative<KHRAnimationPointerTarget::Callback<glm::vec4>>(animationPointerTarget.callback)) {
                        addChannelToAnimation(
                            static_cast<animation::IAnimationTarget<glm::vec4>&>(animationPointerTarget),
                            gltfChannel, animation);
                    } else if (std::holds_alternative<KHRAnimationPointerTarget::Callback<glm::quat>>(animationPointerTarget.callback)) {
                        addChannelToAnimation(
                            static_cast<animation::IAnimationTarget<glm::quat>&>(animationPointerTarget),
                            gltfChannel, animation);
                    } else if (std::holds_alternative<KHRAnimationPointerTarget::Callback<std::vector<float>>>(animationPointerTarget.callback)) {
                        addChannelToAnimation(
                            static_cast<animation::IAnimationTarget<std::vector<float>>&>(animationPointerTarget),
                            gltfChannel, animation);
                    }

                    continue;
                }
                // no extension is used -> only nodes can be animated
                auto targetNodeIt = m_transformHierarchyMapping.map.find(gltfChannel.getTarget().getNode());
                if (targetNodeIt == m_transformHierarchyMapping.map.end()) {
                    continue;
                }

                if (path == gltf::AnimationPath::Translation) {
                    addChannelToAnimation(targetNodeIt->second->translationTarget, gltfChannel, animation);
                } else if (path == gltf::AnimationPath::Rotation) {
                    addChannelToAnimation(targetNodeIt->second->rotationTarget, gltfChannel, animation);
                } else if (path == gltf::AnimationPath::Scale) {
                    addChannelToAnimation(targetNodeIt->second->scaleTarget, gltfChannel, animation);
                } else if (path == gltf::AnimationPath::Weights) {
                    addChannelToAnimation(targetNodeIt->second->weightTarget, gltfChannel, animation);
                }
            }
        }

        return &m_animations.emplace_back(std::move(animation));
    };
    m_animationMapping.destroyRendererobjectFromGltf = [this](const gltf::Animation* gltfAnimation, animation::Animation* animation) {
        auto eraseIt = std::find_if(m_animations.begin(), m_animations.end(), [animation](const auto& x) { return &x == animation; });
        if (eraseIt != m_animations.end()) {
            m_animations.erase(eraseIt);
        }
    };
    m_animationMapping.needsRecreation = [this](animation::Animation* animation, const gltf::Animation* gltfAnimation) -> bool {
        if (animation->isPlaying != gltfAnimation->isPlaying()) {
            return true;
        }
        for (const gltf::AnimationChannel& gltfChannel : gltfAnimation->getChannels()) {
            auto nodeIt = m_transformHierarchyMapping.map.find(gltfChannel.getTarget().getNode());
            if (nodeIt == m_transformHierarchyMapping.map.end()) {
                return true;
            }
            if (gltfChannel.getTarget().m_animationHasChanged) {
                const_cast<gltf::AnimationChannelTarget&>(gltfChannel.getTarget()).m_animationHasChanged = false;
                return true;
            }
        }
        return false;
    };
}

void GltfRenderLayer::updateScene(const gltf::Gltf& gltf, const gltf::Scene& scene)
{
    m_gltfScene = &scene;
    ++m_sceneGeneration;
    ++m_poseGeneration;
    updateTransformHierarchy(gltf.getNodes(), scene.getNodes());

    // ensure transforms from parent nodes are resolved into local transforms (relevant for triangle winding direction)
    m_resourceManager.m_transformHierarchy.apply();
    m_bufferMapping.update(gltf.getAccessors());

    updateLights(scene.getNodes());
    updateMeshes(scene.getNodes());
    updateCameras(scene.getNodes());
    updateAudioEmitters(scene.getNodes());
    updateSkins(scene.getNodes());

    m_animationMapping.update(gltf.getAnimations());

    gl::checkGLError(m_resourceManager.m_gl, "GltfRenderLayer: Error");
}

void GltfRenderLayer::updateGltf(const gltf::Gltf& gltf)
{
    ++m_gltfGeneration;
    // Textures: first create glTextures for all textures present in the gltf
    // We have to scan the material for used textures, as whether the texture is in
    // Linear or sRGB format is only determined by the textures usage in the material.
    // Therefore in the texture mapping the primary key is `std::tuple<gltf::Image*, bool>`
    // where the second element of bool type is true if the texture has linear format.
    std::set<std::tuple<gltf::Image*, bool>> images; // use a set to avoid duplicates
    for (auto& material : gltf.getMaterials()) {
        auto insertImage = [this, &images](const std::optional<gltf::TextureInfo>& textureInfo, bool isLinear) {
            if (textureInfo.has_value() == false) {
                return;
            }

            const gltf::TextureInfo& info(textureInfo.value());
            const gltf::Texture* gltfTexture = info.getTexture();

            if (gltfTexture == nullptr) {
                // in gltf this is not allowed but the renderer should be able to handle this state
                return;
            }

            gltf::Image* gltfImage = nullptr;

            const auto& oBasisU = gltfTexture->getBasisU();
            if (oBasisU.has_value()) {
                gltfImage = oBasisU.value().getSource();
            }

            const auto& oWebP = gltfTexture->getWebP();
            if (oWebP.has_value()) {
                gltfImage = oWebP.value().getSource();
            }

            if (gltfImage == nullptr) {
                // fall back to standard texture source
                gltfImage = gltfTexture->getSource();
            }

            if (gltfImage == nullptr) {
                return;
            }

            images.insert({ gltfImage, isLinear });
        };

        // now access all elements of the material and set the correct linear / sRGB format

        insertImage(material.getNormalTextureInfo(), true);
        insertImage(material.getEmissiveTextureInfo(), false);
        insertImage(material.getOcclusionTextureInfo(), true);

        const auto& metallicRoughness = material.getPbrMetallicRoughness();
        if (metallicRoughness.has_value()) {
            insertImage(metallicRoughness->getBaseColorTextureInfo(), false);
            insertImage(metallicRoughness->getMetallicRoughnessTextureInfo(), true);
        }

        const auto& specularGlossiness = material.getPbrSpecularGlossiness();
        if (specularGlossiness.has_value()) {
            insertImage(specularGlossiness->getSpecularGlossinessTextureInfo(), false);
            insertImage(specularGlossiness->getDiffuseTextureInfo(), false);
        }

        const auto& clearCoat = material.getClearcoat();
        if (clearCoat.has_value()) {
            insertImage(clearCoat->getClearcoatTexture(), true);
            insertImage(clearCoat->getClearcoatRoughnessTexture(), true);
            insertImage(clearCoat->getClearcoatNormalTexture(), true);
        }

        const auto& specular = material.getSpecular();
        if (specular.has_value()) {
            insertImage(specular->getSpecularTextureInfo(), true);
            insertImage(specular->getSpecularColorTextureInfo(), false);
        }

        const auto& sheen = material.getSheen();
        if (sheen.has_value()) {
            insertImage(sheen->getSheenColorTextureInfo(), false);
            insertImage(sheen->getSheenRoughnessTextureInfo(), true);
        }

        const auto& transmission = material.getTransmission();
        if (transmission.has_value()) {
            insertImage(transmission->getTransmissionTextureInfo(), true);
        }

        const auto& volume = material.getVolume();
        if (volume.has_value()) {
            insertImage(volume->getThicknessTextureInfo(), true);
        }

        const auto& iridescence = material.getIridescence();
        if (iridescence.has_value()) {
            insertImage(iridescence->getIridescenceTextureInfo(), true);
            insertImage(iridescence->getIridescenceThicknessTextureInfo(), true);
        }
    }
    core::LinkedList<std::tuple<gltf::Image*, bool>> imagesList; // `update` requires a core::LinkedList
    for (auto& pair : images) {
        imagesList.emplace_back(std::move(pair));
    }
    m_textureMapping.update(imagesList);

    // Buffers: create glBuffers for all Accessorss in the gltf. If you need to reduce memory transfer, try to
    // increase the size of the buffer views in the gltf by for example letting multiple accessors share the samce
    // buffer view
    m_bufferMapping.update(gltf.getAccessors());

    m_samplerStateMapping.update(gltf.getSamplers());

    // Materials: determine the fragment shader and which textures are used within that.
    updateMaterials(gltf.getMaterials());
}

void GltfRenderLayer::applyAnimationTime(const std::optional<float>& animationTime)
{
    const auto profileStart = std::chrono::steady_clock::now();
    ++m_poseGeneration;
    if (animationTime.has_value()) {
        for (animation::Animation& anim : m_animations) {
            anim.updateChannels(*animationTime);
        }
    }
    const auto afterChannels = std::chrono::steady_clock::now();

    m_resourceManager.m_transformHierarchy.apply();
    const auto afterTransforms = std::chrono::steady_clock::now();

    for (auto& skin : m_skins) {
        skin.computeJoints();
    }
    const auto afterSkins = std::chrono::steady_clock::now();

    if (m_cpuProfilingEnabled) {
        const auto toMs = [](auto duration) {
            return std::chrono::duration<double, std::milli>(duration).count();
        };
        m_lastAnimationProfile.channelsMs = toMs(afterChannels - profileStart);
        m_lastAnimationProfile.transformsMs = toMs(afterTransforms - afterChannels);
        m_lastAnimationProfile.skinsMs = toMs(afterSkins - afterTransforms);
    }
}

void GltfRenderLayer::setEnvironmentMap(const filesystem::Path& panoramaIrradiancePath)
{
    if (!gl::checkGLError(m_resourceManager.m_gl, "GltfRenderLayer: Error", false)) {
        return;
    }

    m_resourceManager.destroyAllOf<EnvironmentMap>();
    InitizalizationParameters iblParams;

    iblParams.textureSize = iblMaxResolution;
    m_iblSampler.init(panoramaIrradiancePath, iblParams);

    m_activeEnvironmentMap = m_resourceManager.loadEnvironmentMap(m_iblSampler);
}

bool GltfRenderLayer::filterEnvironmentMap()
{
    return m_iblSampler.filter();
}

void GltfRenderLayer::updateMeshes(const core::Vector<gltf::Node*>& rootNodes)
{
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::Mesh*, const gltf::Primitive*>> gltfTuples;
    // Meshes: in our renderer a mesh is a drawable set of vertices and attributes. It specifies the vertex shader.
    // the gltf correspondence would be 'primitive'
    std::function<void(const gltf::Node&)> gatherMeshRecursive;
    gatherMeshRecursive = [&](const gltf::Node& gltfNode) {
        const gltf::Mesh* gltfMesh = gltfNode.getMesh();
        if (gltfMesh != nullptr) {
            for (const gltf::Primitive& gltfPrimitive : gltfMesh->getPrimitives()) {
                gltfTuples.emplace_back(&gltfNode, gltfMesh, &gltfPrimitive);
            }
        }

        // recurse into children
        for (const gltf::Node* child : gltfNode.getChildren()) {
            gatherMeshRecursive(*child);
        }
    };

    // start the gathering process on all root nodes
    for (const gltf::Node* gltfNode : rootNodes) {
        gatherMeshRecursive(*gltfNode);
    }

    m_meshMapping.update(gltfTuples);
}

void GltfRenderLayer::updateMaterials(const core::LinkedList<gltf::Material>& materials)
{
    core::LinkedList<const gltf::Material*> gltfMaterials;
    for (const auto& gltfMat : materials) {
        gltfMaterials.emplace_back(&gltfMat);
    }

    // insert a default material
    gltfMaterials.emplace_back(nullptr);

    m_materialMapping.update(gltfMaterials);
}
void GltfRenderLayer::updateSkins(const core::Vector<gltf::Node*>& rootNodes)
{
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::Skin*>> gltfTuples;
    std::function<void(const gltf::Node&)> gatherSkinRecursive;
    gatherSkinRecursive = [&](const gltf::Node& gltfNode) {
        const gltf::Skin* gltfSkin = gltfNode.getSkin();
        const gltf::Mesh* gltfMesh = gltfNode.getMesh();
        if (gltfSkin != nullptr && gltfMesh != nullptr && !gltfMesh->getPrimitives().empty()) {
            gltfTuples.emplace_back(&gltfNode, gltfSkin);
        }

        // recurse into children
        for (const gltf::Node* child : gltfNode.getChildren()) {
            gatherSkinRecursive(*child);
        }
    };
    for (const gltf::Node* gltfNode : rootNodes) {
        gatherSkinRecursive(*gltfNode);
    }
    m_skinMapping.update(gltfTuples);
}

namespace {

// The primitive's local POSITION box, or false when it cannot bound what the vertex
// shader draws: skinning and morphing move vertices, and min/max of a non-float
// (quantized) accessor are not in model units.
bool getStaticPositionBounds(const gltf::Primitive& primitive, const Mesh& mesh, math::vec3& lo, math::vec3& hi)
{
    if (!primitive.getTargets().empty() || mesh.defines.isDefined("USE_SKINNING") || mesh.defines.isDefined("USE_MORPHING")
        || mesh.vertexShader != "primitive.vert") {
        return false;
    }
    for (const gltf::Attribute& attribute : primitive.getAttributes()) {
        if (attribute.getName() != "POSITION") {
            continue;
        }
        const gltf::Accessor* accessor = attribute.getAccessor();
        if (accessor == nullptr || accessor->getComponentType() != gltf::ComponentType::Float
            || accessor->getMin().size() != 3 || accessor->getMax().size() != 3) {
            return false;
        }
        lo = math::vec3(static_cast<float>(accessor->getMin()[0]), static_cast<float>(accessor->getMin()[1]), static_cast<float>(accessor->getMin()[2]));
        hi = math::vec3(static_cast<float>(accessor->getMax()[0]), static_cast<float>(accessor->getMax()[1]), static_cast<float>(accessor->getMax()[2]));
        return true;
    }
    return false;
}

// True when all eight corners of the box, in clip space, lie strictly outside one of
// the six clip half-spaces (-w <= x, y, z <= w). Each half-space is linear in
// homogeneous coordinates, so this holds for the whole box, including corners behind
// the camera, under perspective, orthographic and infinite projections and any model
// matrix (negative or non-uniform scale). A small relative margin keeps boxes that
// touch a plane.
bool boxOutsideClip(const math::mat4x4& clipFromModel, const math::vec3& lo, const math::vec3& hi)
{
    constexpr float kMargin = 1e-4f;
    uint32_t outside[6] = { 0, 0, 0, 0, 0, 0 };
    for (int corner = 0; corner < 8; ++corner) {
        const math::vec4 p = clipFromModel * math::vec4((corner & 1) ? hi.x : lo.x, (corner & 2) ? hi.y : lo.y, (corner & 4) ? hi.z : lo.z, 1.0f);
        const float w = p.w + kMargin * std::abs(p.w);
        outside[0] += p.x < -w;
        outside[1] += p.x > w;
        outside[2] += p.y < -w;
        outside[3] += p.y > w;
        outside[4] += p.z < -w;
        outside[5] += p.z > w;
    }
    for (uint32_t count : outside) {
        if (count == 8) {
            return true;
        }
    }
    return false;
}

} // namespace

std::tuple<RendererScene, ReverseMapping> GltfRenderLayer::createScene(const gltf::Scene* gltfScene)
{
    RendererScene scene;
    ReverseMapping reverseMapping;
    bool containsTransmissiveMaterial = false;
    m_gltfScene = gltfScene;
    m_lastSceneStats = SceneProfileStats();
    const bool cull = frustumCulling && m_activeCamera != nullptr;
    const math::mat4x4 clipFromWorld = cull ? m_activeCamera->getProjectionMatrix() * m_activeCamera->getViewMatrix() : math::mat4x4(1.0f);

    std::function<void(const gltf::Node*)> addMeshMaterialRecursive;
    addMeshMaterialRecursive = [&](const gltf::Node* gltfNode) {
        ++m_lastSceneStats.nodesVisited;
        for (const auto* child : gltfNode->getChildren()) {
            addMeshMaterialRecursive(child);
        }

        const gltf::Light* gltfLight = gltfNode->getLight();
        if (gltfLight != nullptr) {
            auto lightIt = m_lightMapping.map.find(std::tuple(gltfNode, gltfLight));
            if (lightIt != m_lightMapping.map.end()) {
                scene.lights.emplace_back(std::get<Light*>(lightIt->second));
            }
        }

        const auto* gltfMesh = gltfNode->getMesh();
        if (gltfMesh == nullptr)
            return;

        if (!gltfNode->getVisible() || !gltfMesh->getVisible())
            return;

        for (auto& gltfPrimitive : gltfMesh->getPrimitives()) {
            ++m_lastSceneStats.primitivesVisited;
            // TODO: use new callback to sort commands into render passes
            if (!gltfPrimitive.getVisible())
                continue;

            auto gltfTuple = std::tuple(gltfNode, gltfMesh, &gltfPrimitive);
            auto meshIt = m_meshMapping.map.find(gltfTuple);
            if (meshIt == m_meshMapping.map.end())
                continue;

            const gltf::Material* gltfMaterial;
            if (gltfPrimitive.hasMaterialVariantsMapping()
                && m_materialVariantID.has_value()
                && gltfPrimitive.getMaterialVariantsMapping().value().hasVariant(m_materialVariantID.value())) {
                gltfMaterial = gltfPrimitive.getMaterialVariantsMapping()->getMaterial(m_materialVariantID.value());
            } else {
                gltfMaterial = gltfPrimitive.getMaterial();
            }

            auto matIt = m_materialMapping.map.find(gltfMaterial);
            if (matIt == m_materialMapping.map.end())
                continue;

            if (gltfMaterial != nullptr) {
                if (gltfMaterial->getTransmission().has_value()) {
                    containsTransmissiveMaterial = true;
                }
            }

            auto* mesh = meshIt->second;
            auto* material = matIt->second;

            if (cull) {
                math::vec3 lo;
                math::vec3 hi;
                if (!getStaticPositionBounds(gltfPrimitive, *mesh, lo, hi)) {
                    ++m_lastSceneStats.uncullable;
                } else if (boxOutsideClip(clipFromWorld * mesh->getTransform(), lo, hi)) {
                    ++m_lastSceneStats.culled;
                    continue;
                }
            }

            mesh->defines.define("DEBUG", static_cast<int>(RenderMode::None));
            mesh->polygonMode = gl::PolygonMode::Fill;
            material->fragmentShader = "pbr.frag";
            material->renderSettings = renderSettings;

            scene.drawables.emplace_back(mesh, material);
            reverseMapping[meshIt->second] = gltfNode;
        }
    };

    if (gltfScene != nullptr) {
        for (const gltf::Node* gltfNode : gltfScene->getNodes()) {
            addMeshMaterialRecursive(gltfNode);
        }
    }

    scene.camera = m_activeCamera;
    scene.environmentMap = getActiveEnvironmentMap();

    for (const auto [mesh, material] : scene.drawables) {
        if (mesh->defines.isDefined("USE_MORPHING")) {
            mesh->defines.defineIf(renderSettings.enableMorphing, "USE_MORPHING");
        }
        if (!mesh->defines.isDefined("WEIGHT_COUNT")) {
            mesh->defines.define("WEIGHT_COUNT", 1); // Must be positive (with 0 there are compilation errors)
        }
        mesh->defines.define("DEBUG", static_cast<int>(RenderMode::None));
        mesh->polygonMode = gl::PolygonMode::Fill;
        material->fragmentShader = "pbr.frag";
        material->renderSettings = renderSettings;
    }

    // add CubeMap
    if (m_cubeMapEnabled && m_activeCamera != nullptr && m_activeCamera->getType() == Camera::Type::Perspective) {
        scene.drawables.emplace_back(m_resourceManager.getCubeMapTuple());
    }

    // Previous transmission duplicates are reused from the pools below.
    m_postTransmissionDrawables.clear();
    size_t transmissionPoolUsed = 0;

    // Duplicate drawables for transmission path
    if (containsTransmissiveMaterial) {
        for (const auto [mesh, material] : scene.drawables) {
            if (material->renderPass->getId() != Opaque
                && material->renderPass->getId() != Transparent
                && material->renderPass->getId() != Grid) {
                continue;
            }

            Material* materialDuplicate = nullptr;
            RenderPass* renderPassDuplicate = nullptr;
            if (transmissionPoolUsed < m_transmissionMaterialPool.size()) {
                materialDuplicate = m_transmissionMaterialPool[transmissionPoolUsed].get();
                *materialDuplicate = *material;
                renderPassDuplicate = m_transmissionPassPool[transmissionPoolUsed].get();
                *renderPassDuplicate = *material->renderPass;
            } else {
                m_transmissionMaterialPool.push_back(std::make_unique<Material>(*material));
                m_transmissionPassPool.push_back(std::make_unique<RenderPass>(*material->renderPass));
                materialDuplicate = m_transmissionMaterialPool.back().get();
                renderPassDuplicate = m_transmissionPassPool.back().get();
            }
            ++transmissionPoolUsed;
            materialDuplicate->defines.define("LINEAR_OUTPUT", 1);
            materialDuplicate->renderPass = renderPassDuplicate;

            if (material->renderPass->getId() == Opaque) {
                renderPassDuplicate->setId(RenderPassType::Opaque2);
            } else if (material->renderPass->getId() == Transparent) {
                renderPassDuplicate->setId(RenderPassType::Transparent2);
            } else if (material->renderPass->getId() == Grid) {
                renderPassDuplicate->setId(RenderPassType::Grid2);
            }

            m_postTransmissionDrawables.emplace_back(mesh, materialDuplicate);
        }

        scene.drawables.concat(m_postTransmissionDrawables);
        scene.drawables.emplace_back(m_resourceManager.getEmptyTransmissionBlit());
    }

    m_lastSceneStats.drawables = static_cast<uint64_t>(scene.drawables.size());
    return { scene, reverseMapping };
}

void GltfRenderLayer::updateCameras(const core::Vector<gltf::Node*>& cameraNodes)
{
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::Camera*>> gltfTuples;

    std::function<void(const gltf::Node*)> gatherNodesRecursive;
    gatherNodesRecursive = [&](const gltf::Node* gltfNode) {
        const gltf::Camera* gltfCamera = gltfNode->getCamera();
        if (gltfCamera != nullptr) {
            gltfTuples.emplace_back(gltfNode, gltfCamera);
        }

        for (const gltf::Node* gltfChild : gltfNode->getChildren()) {
            gatherNodesRecursive(gltfChild);
        }
    };

    for (const gltf::Node* gltfNode : cameraNodes) {
        gatherNodesRecursive(gltfNode);
    }

    m_cameraMapping.update(gltfTuples);
}

void GltfRenderLayer::updateLights(const core::Vector<gltf::Node*>& lightNodes)
{
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::Light*>> gltfTuples;
    std::function<void(const gltf::Node*)> gatherNodesRecursive;
    gatherNodesRecursive = [&](const gltf::Node* gltfNode) {
        const gltf::Light* pGltfLight = gltfNode->getLight();
        if (pGltfLight != nullptr && gltfNode->getVisible()) {
            gltfTuples.emplace_back(gltfNode, pGltfLight);
        }

        for (const gltf::Node* gltfChild : gltfNode->getChildren()) {
            gatherNodesRecursive(gltfChild);
        }
    };

    for (const gltf::Node* gltfNode : lightNodes) {
        gatherNodesRecursive(gltfNode);
    }

    m_lightMapping.update(gltfTuples);
}

void GltfRenderLayer::updateAudioEmitters(const core::Vector<gltf::Node*>& audioNodes)
{
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::AudioEmitter*>> gltfTuples;
    std::function<void(const gltf::Node*)> gatherNodesRecursive;
    gatherNodesRecursive = [&](const gltf::Node* node) {
        const gltf::AudioEmitter* audio = node->getAudioEmitter();
        if (audio != nullptr && node->getVisible()) {
            gltfTuples.emplace_back(node, audio);
        }

        for (const gltf::Node* gltfChild : node->getChildren()) {
            gatherNodesRecursive(gltfChild);
        }
    };

    for (const gltf::Node* gltfNode : audioNodes) {
        gatherNodesRecursive(gltfNode);
    }

    m_audioMapping.update(gltfTuples);
}

void GltfRenderLayer::updateTransformHierarchy(const core::LinkedList<gltf::Node>& gltfNodes, const core::Vector<gltf::Node*>& gltfRootNodes)
{
    m_resourceManager.m_transformHierarchy.clear();

    m_transformHierarchyMapping.update(gltfNodes);

    // place the nodes in the transform hierarchy
    std::function<renderer::TransformHierarchyNode*(const gltf::Node*)> gatherTransformHierarchy;
    gatherTransformHierarchy = [&](const gltf::Node* gltfNode) -> renderer::TransformHierarchyNode* {
        auto node = m_transformHierarchyMapping.map[gltfNode];
        for (const gltf::Node* gltfChild : gltfNode->getChildren()) {
            node->addChild(gatherTransformHierarchy(gltfChild));
        }
        return node;
    };
    for (const gltf::Node* gltfRootNode : gltfRootNodes) {
        m_resourceManager.m_transformHierarchy.addRootNode(gatherTransformHierarchy(gltfRootNode));
    }
}

void GltfRenderLayer::setActiveGltfCameraByNode(const gltf::Node* _CameraNode)
{
    auto cameraIt = std::find_if(m_cameraMapping.map.begin(), m_cameraMapping.map.end(), [_CameraNode](auto& pair) {
        return std::get<const gltf::Node*>(pair.first) == _CameraNode;
    });

    if (cameraIt != m_cameraMapping.map.end()) {
        m_activeCamera = std::get<Camera*>(cameraIt->second);
    } else {
        m_activeCamera = nullptr;
    }
}

const gltf::Node* GltfRenderLayer::getActiveGltfCameraNode() const
{
    auto cameraIt = std::find_if(m_cameraMapping.map.begin(), m_cameraMapping.map.end(), [this](auto& pair) {
        return std::get<Camera*>(pair.second) == m_activeCamera;
    });

    if (cameraIt != m_cameraMapping.map.end()) {
        return std::get<const gltf::Node*>(cameraIt->first);
    }

    return nullptr;
}

const gltf::Camera* GltfRenderLayer::getActiveGltfCamera() const
{
    auto cameraIt = std::find_if(m_cameraMapping.map.begin(), m_cameraMapping.map.end(), [this](auto& pair) {
        return std::get<Camera*>(pair.second) == m_activeCamera;
    });

    if (cameraIt != m_cameraMapping.map.end()) {
        return std::get<const gltf::Camera*>(cameraIt->first);
    }

    return nullptr;
}

math::mat4 GltfRenderLayer::getWorldTransform(const gltf::Node* searchNode) const
{
    if (searchNode) {
        const auto& result = m_transformHierarchyMapping.map.find(searchNode);
        if (result != m_transformHierarchyMapping.map.end()) {
            return result->second->getWorldTransform();
        }
    }

    return math::mat4(1.0);
}

math::mat3x3 GltfRenderLayer::getWorldRotation(const gltf::Node* searchNode) const
{
    if (searchNode) {
        const auto& result = m_transformHierarchyMapping.map.find(searchNode);
        if (result != m_transformHierarchyMapping.map.end()) {
            return result->second->getWorldRotation();
        }
    }

    return math::mat4(1.0);
    ;
}

math::quat GltfRenderLayer::getWorldRotationQuat(const gltf::Node* searchNode) const
{
    return math::toQuat(getWorldRotation(searchNode));
}

math::mat4 GltfRenderLayer::getParentWorldTransform(const gltf::Node* searchNode) const
{
    if (searchNode) {
        for (const auto& walker : m_transformHierarchyMapping.map) {
            for (const auto& childWalker : walker.first->getChildren()) {
                if (childWalker == searchNode) {
                    return walker.second->getWorldTransform();
                }
            }
        }
    }

    return math::mat4(1.0);
}

math::mat3x3 GltfRenderLayer::getParentWorldRotation(const gltf::Node* searchNode) const
{
    if (searchNode) {
        for (const auto& walker : m_transformHierarchyMapping.map) {
            for (const auto& childWalker : walker.first->getChildren()) {
                if (childWalker == searchNode) {
                    return walker.second->getWorldRotation();
                }
            }
        }
    }

    return math::mat4(1.0);
    ;
}

math::quat GltfRenderLayer::getParentWorldRotationQuat(const gltf::Node* searchNode) const
{
    return math::toQuat(getParentWorldRotation(searchNode));
}

float GltfRenderLayer::getAnimationDuration() const
{
    float maximum = 0.0f;

    for (auto& animation : m_animations) {
        if (animation.isPlaying) {
            maximum = math::max(maximum, animation.duration);
        }
    }

    return maximum;
}

Camera* GltfRenderLayer::addCamera()
{
    return m_resourceManager.createWithGLContext<renderer::Camera>();
}
