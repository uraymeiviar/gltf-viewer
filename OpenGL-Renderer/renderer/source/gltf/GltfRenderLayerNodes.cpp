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
        if (interpolation != gltf::AnimationInterpolation::CubicSpline) {
            const auto offset = i * valueCount;
            for (uint32_t v = 0u; v < valueCount; ++v) {
                values[v] = outputValues[offset + v];
            }
            channel.addKeyframe(keys[i], values);
        } else {
            // glTF: per key all in-tangents, then all values, then all out-tangents (valueCount each)
            const auto offset = i * valueCount * 3u;
            for (uint32_t v = 0u; v < valueCount; ++v) {
                inTangents[v] = outputValues[offset + v];
                values[v] = outputValues[offset + valueCount + v];
                outTangents[v] = outputValues[offset + 2u * valueCount + v];
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

        // KHR_node_visibility: a sync restores the file's value; a KHR_animation_pointer channel (boolean: unsigned
        // byte 0 / 1, STEP) sets the node's current value on each animation apply. The traversal reads it every frame,
        // so a toggle rebuilds nothing. Only a node that carries the extension is a target (switched off: read as
        // absent, the channel is skipped and the node stays visible).
        gltfNode->resetAnimatedFileVisible();
        if (gltfNode->hasNodeVisibility()) {
            m_animationPointerTargets[{ gltfNode, gltf::node_visibility::PointerPath }].callback = KHRAnimationPointerTarget::Callback<float> {
                [gltfNode](const float& value) -> void {
                    gltfNode->setAnimatedFileVisible(value != 0.0f);
                }
            };
        } else {
            m_animationPointerTargets.erase({ gltfNode, gltf::node_visibility::PointerPath });
        }

        // KHR_node_selectability / KHR_node_hoverability: the same per-frame flags (boolean channels).
        gltfNode->resetAnimatedSelectable();
        gltfNode->resetAnimatedHoverable();
        if (gltfNode->hasNodeSelectability()) {
            m_animationPointerTargets[{ gltfNode, gltf::node_interaction::SelectablePointer }].callback = KHRAnimationPointerTarget::Callback<float> {
                [gltfNode](const float& value) -> void {
                    gltfNode->setAnimatedSelectable(value != 0.0f);
                }
            };
        } else {
            m_animationPointerTargets.erase({ gltfNode, gltf::node_interaction::SelectablePointer });
        }
        if (gltfNode->hasNodeHoverability()) {
            m_animationPointerTargets[{ gltfNode, gltf::node_interaction::HoverablePointer }].callback = KHRAnimationPointerTarget::Callback<float> {
                [gltfNode](const float& value) -> void {
                    gltfNode->setAnimatedHoverable(value != 0.0f);
                }
            };
        } else {
            m_animationPointerTargets.erase({ gltfNode, gltf::node_interaction::HoverablePointer });
        }

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
        m_animationPointerTargets.erase({ gltfNode, gltf::node_visibility::PointerPath });
        m_animationPointerTargets.erase({ gltfNode, gltf::node_interaction::SelectablePointer });
        m_animationPointerTargets.erase({ gltfNode, gltf::node_interaction::HoverablePointer });
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

        // Bind convention. Strict glTF ignores the skinned mesh node's transform: at rest,
        // jointWorld * IBM equals the mesh node's world matrix (or varies per joint when the
        // rest pose is not the bind pose). Asobo/MSFS exports keep the vertices local to a placed
        // mesh node and leave its transform out of the IBMs: at rest every jointWorld * IBM is
        // one shared matrix C (usually the identity) that is not the mesh node's. Strictly, those
        // vertices land near C * v and pivot about joints metres away once a bone moves (torn
        // yoke cables and boots); Blender and MSFS deform the placed mesh instead.
        if (const auto meshRestIt = m_skinRestWorld.find(gltfNode); meshRestIt != m_skinRestWorld.end()) {
            const math::dmat4x4& meshRest = meshRestIt->second;
            const auto& gltfJoints = gltfSkin->getJoints();
            double scale = 1.0;
            for (int c = 0; c < 3; ++c) {
                scale = std::max(scale, std::abs(meshRest[3][c]));
            }
            const double tolerance = 1e-3 * scale;
            const auto maxDifference = [](const math::dmat4x4& a, const math::dmat4x4& b) {
                double result = 0.0;
                for (int c = 0; c < 4; ++c) {
                    for (int r = 0; r < 4; ++r) {
                        result = std::max(result, std::abs(a[c][r] - b[c][r]));
                    }
                }
                return result;
            };
            bool shared = !gltfJoints.empty() && gltfJoints.size() == skin.getInverseBindMatrixCount();
            math::dmat4x4 restBind(1.0);
            for (size_t j = 0; shared && j < gltfJoints.size(); ++j) {
                const auto jointRestIt = m_skinRestWorld.find(gltfJoints[j]);
                if (jointRestIt == m_skinRestWorld.end()) {
                    shared = false;
                    break;
                }
                const math::dmat4x4 bind = jointRestIt->second * math::dmat4x4(skin.getInverseBindMatrix(j));
                if (j == 0) {
                    restBind = bind;
                } else if (maxDifference(bind, restBind) > tolerance) {
                    shared = false;
                }
            }
            // A mesh node at the identity is the strict-glTF layout (an armature moved after
            // binding moves the mesh with it), so only a placed mesh node switches convention.
            if (shared && maxDifference(restBind, meshRest) > tolerance && maxDifference(meshRest, math::dmat4x4(1.0)) > tolerance) {
                skin.setBindCorrection(math::mat4x4(math::inverse(restBind) * meshRest));
                core::logInfo("Skin '%s' on node '%s': inverse bind matrices omit the mesh node transform; "
                              "the placed mesh is deformed when the skinned mesh node transform setting applies it "
                              "(Blender/MSFS; Auto: files with ASOBO extensions)",
                    gltfSkin->getName().c_str(), gltfNode->getName().c_str());
            }
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

    const bool correct = skinNodeTransform == SkinNodeTransform::MeshNode ||
                         (skinNodeTransform == SkinNodeTransform::Auto && m_assetHasAsoboExtensions);
    for (auto& skin : m_skins) {
        skin.setBindCorrectionActive(correct);
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

size_t GltfRenderLayer::detectedSkinCorrections() const
{
    return static_cast<size_t>(std::count_if(m_skins.begin(), m_skins.end(), [](const renderer::Skin& s) { return s.hasBindCorrection(); }));
}

size_t GltfRenderLayer::appliedSkinCorrections() const
{
    return static_cast<size_t>(std::count_if(m_skins.begin(), m_skins.end(), [](const renderer::Skin& s) { return s.bindCorrectionApplied(); }));
}

void GltfRenderLayer::updateSkins(const core::Vector<gltf::Node*>& rootNodes)
{
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::Skin*>> gltfTuples;
    m_skinRestWorld.clear();
    std::function<void(const gltf::Node&, const math::dmat4x4&)> gatherSkinRecursive;
    gatherSkinRecursive = [&](const gltf::Node& gltfNode, const math::dmat4x4& parentWorld) {
        const math::dmat4x4 world = parentWorld * gltfNode.getTransformMatrix();
        m_skinRestWorld[&gltfNode] = world;
        const gltf::Skin* gltfSkin = gltfNode.getSkin();
        const gltf::Mesh* gltfMesh = gltfNode.getMesh();
        if (gltfSkin != nullptr && gltfMesh != nullptr && !gltfMesh->getPrimitives().empty()) {
            gltfTuples.emplace_back(&gltfNode, gltfSkin);
        }

        // recurse into children
        for (const gltf::Node* child : gltfNode.getChildren()) {
            gatherSkinRecursive(*child, world);
        }
    };
    for (const gltf::Node* gltfNode : rootNodes) {
        gatherSkinRecursive(*gltfNode, math::dmat4x4(1.0));
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

