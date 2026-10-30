// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// GltfRenderLayer nodes: the transform hierarchy, skin and animation mappings (animation channels from glTF
// samplers), their updates, animation time and duration.

#include "GltfRenderLayerInternal.h"
// Sampler.h defines explicit (non-inline) specialisations: include it in this one file only.
#include "animation/Sampler.h"

namespace ux3d::renderer {

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

void GltfRenderLayer::initNodeMappings(gl::GL* gl)
{
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

