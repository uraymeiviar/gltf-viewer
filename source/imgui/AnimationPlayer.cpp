// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/AnimationPlayer.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "gltf/animation/AnimationPath.h"
#include <imgui.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>
#include <algorithm>

namespace ux3d::imgui {

void AnimationPlayer::reset()
{
    m_isPlaying = false;
    m_currentTime = 0.0f;
    m_maxDuration = 0.0f;
    m_activeAnimationIdx = 0;
    m_currentLoop = 0;
    invalidateActiveConflictCounts();
}

void AnimationPlayer::setModel(ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    reset();
    selectAllPossibleAnimations(true, gltfModel, gltfLayer);
}

bool AnimationPlayer::checkConflict(const ux3d::gltf::Animation& animA, const ux3d::gltf::Animation& animB) const
{
    for (const auto& chA : animA.getChannels()) {
        const ux3d::gltf::ChildOfRoot* nodeA = chA.getTarget().getNode();
        std::string pathA = ux3d::gltf::animationPathToString(chA.getTarget().getPath()).c_str();
        if (chA.getTarget().m_animationPointer.has_value()) {
            nodeA = std::get<0>(chA.getTarget().m_animationPointer.value());
            pathA = std::get<1>(chA.getTarget().m_animationPointer.value());
        }

        for (const auto& chB : animB.getChannels()) {
            const ux3d::gltf::ChildOfRoot* nodeB = chB.getTarget().getNode();
            std::string pathB = ux3d::gltf::animationPathToString(chB.getTarget().getPath()).c_str();
            if (chB.getTarget().m_animationPointer.has_value()) {
                nodeB = std::get<0>(chB.getTarget().m_animationPointer.value());
                pathB = std::get<1>(chB.getTarget().m_animationPointer.value());
            }

            if (nodeA == nodeB && pathA == pathB && (nodeA != nullptr || !pathA.empty())) {
                return true;
            }
        }
    }
    return false;
}

void AnimationPlayer::ensureConflictIndex(const ux3d::gltf::Gltf& gltfModel) const
{
    const auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    const uint64_t targetRevision = ux3d::gltf::AnimationChannelTarget::getTargetRevision();
    const uint64_t playingRevision = ux3d::gltf::Animation::getPlayingRevision();
    const size_t animationCount = animations.size();

    const bool clipsStale = !m_conflictCache.built
        || m_conflictCache.model != &gltfModel
        || m_conflictCache.modelRevision != gltfModel.getRevision()
        || m_conflictCache.targetRevision != targetRevision
        || m_conflictCache.animationCount != animationCount;

    if (clipsStale) {
        m_conflictCache.model = &gltfModel;
        m_conflictCache.modelRevision = gltfModel.getRevision();
        m_conflictCache.targetRevision = targetRevision;
        m_conflictCache.animationCount = animationCount;
        m_conflictCache.clips.clear();
        m_conflictCache.clips.resize(animationCount);
        m_conflictCache.activeTargetCounts.clear();
        m_conflictCache.activeCountsValid = false;
        m_conflictCache.built = true;

        for (size_t animationIndex = 0; animationIndex < animationCount; ++animationIndex) {
            const auto& animation = animations.at(animationIndex);
            std::unordered_set<ConflictTarget, ConflictTargetHash> uniqueTargets;
            uniqueTargets.reserve(animation.getChannels().size());

            for (const auto& channel : animation.getChannels()) {
                const ConflictTarget target = [&channel]() {
                    ConflictTarget result;
                    result.object = channel.getTarget().getNode();
                    result.path = ux3d::gltf::animationPathToString(channel.getTarget().getPath()).c_str();
                    if (channel.getTarget().m_animationPointer.has_value()) {
                        result.object = std::get<0>(channel.getTarget().m_animationPointer.value());
                        result.path = std::get<1>(channel.getTarget().m_animationPointer.value());
                    }
                    return result;
                }();

                if (target.object != nullptr || !target.path.empty()) {
                    uniqueTargets.insert(target);
                }
            }

            auto& clip = m_conflictCache.clips[animationIndex];
            clip.targets.assign(uniqueTargets.begin(), uniqueTargets.end());
        }
    }

    if (m_conflictCache.playingRevision != playingRevision) {
        m_conflictCache.playingRevision = playingRevision;
        m_conflictCache.activeCountsValid = false;
    }
}

void AnimationPlayer::rebuildActiveConflictCounts(const ux3d::gltf::Gltf& gltfModel) const
{
    ensureConflictIndex(gltfModel);

    m_conflictCache.activeTargetCounts.clear();
    const auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    for (size_t animationIndex = 0; animationIndex < m_conflictCache.clips.size(); ++animationIndex) {
        if (!animations.at(animationIndex).isPlaying()) {
            continue;
        }
        for (const auto& target : m_conflictCache.clips[animationIndex].targets) {
            ++m_conflictCache.activeTargetCounts[target];
        }
    }
    m_conflictCache.playingRevision = ux3d::gltf::Animation::getPlayingRevision();
    m_conflictCache.activeCountsValid = true;
}

void AnimationPlayer::invalidateActiveConflictCounts() const
{
    m_conflictCache.activeTargetCounts.clear();
    m_conflictCache.activeCountsValid = false;
}

bool AnimationPlayer::targetConflictsWithActive(const ConflictTarget& target, bool selfIsPlaying) const
{
    const auto it = m_conflictCache.activeTargetCounts.find(target);
    if (it == m_conflictCache.activeTargetCounts.end()) {
        return false;
    }
    return it->second > (selfIsPlaying ? 1u : 0u);
}

bool AnimationPlayer::isAnimationConflictingWithActive(size_t index, const ux3d::gltf::Gltf& gltfModel) const
{
    const auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    if (index >= animations.size()) return false;

    ensureConflictIndex(gltfModel);
    if (!m_conflictCache.activeCountsValid) {
        rebuildActiveConflictCounts(gltfModel);
    }

    if (index >= m_conflictCache.clips.size()) return false;
    const bool selfIsPlaying = animations.at(index).isPlaying();
    for (const auto& target : m_conflictCache.clips[index].targets) {
        if (targetConflictsWithActive(target, selfIsPlaying)) {
            return true;
        }
    }
    return false;
}

bool AnimationPlayer::areAllPossibleAnimationsSelected(const ux3d::gltf::Gltf& gltfModel) const
{
    const auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    if (animations.empty()) return false;

    for (size_t i = 0; i < animations.size(); ++i) {
        const auto& anim = animations.at(i);
        if (!anim.isPlaying()) {
            if (!isAnimationConflictingWithActive(i, gltfModel)) {
                return false;
            }
        }
    }
    return true;
}

void AnimationPlayer::calculateMaxDuration(ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    m_maxDuration = 0.0f;
    if (gltfLayer != nullptr) {
        m_maxDuration = gltfLayer->getAnimationDuration();
    }
    if (m_maxDuration <= 0.0f) {
        auto& anims = gltfModel.getElements<ux3d::gltf::Animation>();
        for (const auto& anim : anims) {
            for (const auto& ch : anim.getChannels()) {
                if (ch.getSampler() != nullptr && ch.getSampler()->getInputAccessor() != nullptr) {
                    auto* acc = ch.getSampler()->getInputAccessor();
                    if (!acc->getMax().empty()) {
                        float t = static_cast<float>(acc->getMax()[0]);
                        if (t > m_maxDuration) {
                            m_maxDuration = t;
                        }
                    }
                }
            }
        }
    }
}

void AnimationPlayer::selectAllPossibleAnimations(bool select, ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    if (animations.empty()) return;

    ensureConflictIndex(gltfModel);
    m_conflictCache.activeTargetCounts.clear();
    m_conflictCache.activeCountsValid = true;

    if (select) {
        for (size_t animationIndex = 0; animationIndex < animations.size(); ++animationIndex) {
            const auto& clip = m_conflictCache.clips[animationIndex];
            bool canPlay = true;
            for (const auto& target : clip.targets) {
                if (targetConflictsWithActive(target, false)) {
                    canPlay = false;
                    break;
                }
            }

            animations.at(animationIndex).setPlaying(canPlay);
            if (canPlay) {
                for (const auto& target : clip.targets) {
                    ++m_conflictCache.activeTargetCounts[target];
                }
            }
        }
    } else {
        for (auto& anim : animations) {
            anim.setPlaying(false);
        }
    }

    m_conflictCache.playingRevision = ux3d::gltf::Animation::getPlayingRevision();
    m_conflictCache.activeCountsValid = true;

    if (gltfLayer != nullptr) {
        gltfLayer->m_animationMapping.update(gltfModel.getAnimations());
        calculateMaxDuration(gltfModel, gltfLayer);
        gltfLayer->applyAnimationTime(m_currentTime);
    }
}

void AnimationPlayer::toggleAnimation(size_t index, bool playing, ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    if (index >= animations.size()) return;

    if (playing) {
        if (!isAnimationConflictingWithActive(index, gltfModel)) {
            animations.at(index).setPlaying(true);
        }
    } else {
        animations.at(index).setPlaying(false);
    }
    rebuildActiveConflictCounts(gltfModel);

    if (gltfLayer != nullptr) {
        gltfLayer->m_animationMapping.update(gltfModel.getAnimations());
        calculateMaxDuration(gltfModel, gltfLayer);
        gltfLayer->applyAnimationTime(m_currentTime);
    }
}

void AnimationPlayer::setActiveAnimationIndex(int idx, ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    if (idx < 0 || idx >= static_cast<int>(animations.size())) return;

    m_activeAnimationIdx = idx;
    for (size_t i = 0; i < animations.size(); ++i) {
        animations.at(i).setPlaying(static_cast<int>(i) == idx);
    }
    rebuildActiveConflictCounts(gltfModel);

    if (gltfLayer != nullptr) {
        gltfLayer->m_animationMapping.update(gltfModel.getAnimations());
        calculateMaxDuration(gltfModel, gltfLayer);
        gltfLayer->applyAnimationTime(m_currentTime);
    }
}

void AnimationPlayer::stop(ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    m_isPlaying = false;
    m_currentTime = 0.0f;
    m_currentLoop = 0;
    if (gltfLayer != nullptr) {
        gltfLayer->applyAnimationTime(0.0f);
    }
}

void AnimationPlayer::setCurrentTime(float t, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    m_currentTime = t;
    if (gltfLayer != nullptr) {
        gltfLayer->applyAnimationTime(m_currentTime);
    }
}

SelfCheckResult AnimationPlayer::runConflictSelfCheck(bool forceFailure)
{
    using ux3d::gltf::Animation;
    using ux3d::gltf::AnimationPath;
    using ux3d::gltf::Gltf;
    using ux3d::gltf::Node;

    int checks = 0;
    std::vector<std::string> failures;

    auto check = [&](bool condition, const std::string& message) {
        ++checks;
        if (!condition && failures.size() < 20) {
            failures.push_back(message);
        }
    };

    auto makeCaseModel = []() {
        auto model = std::make_unique<Gltf>();
        Node* n0 = model->create<Node>();
        Node* n1 = model->create<Node>();
        Node* n2 = model->create<Node>();
        n0->setName("N0");
        n1->setName("N1");
        n2->setName("N2");

        auto addChannel = [](Animation& animation, Node& node, AnimationPath path) {
            auto* channel = animation.createChannel();
            channel->getTarget().setNode(node);
            channel->getTarget().setPath(path);
        };
        auto addNullChannel = [](Animation& animation, AnimationPath path) {
            auto* channel = animation.createChannel();
            channel->getTarget().setPath(path);
        };
        auto addPointerChannel = [](Animation& animation, ux3d::gltf::ChildOfRoot& object, const std::string& path) {
            auto* channel = animation.createChannel();
            channel->getTarget().setAnimationPointer(std::make_tuple(&object, path));
        };

        Animation* a = model->create<Animation>();
        a->setName("A");
        addChannel(*a, *n0, AnimationPath::Translation);
        addChannel(*a, *n1, AnimationPath::Rotation);

        Animation* b = model->create<Animation>();
        b->setName("B");
        addChannel(*b, *n0, AnimationPath::Translation);
        addChannel(*b, *n0, AnimationPath::Translation);
        addChannel(*b, *n2, AnimationPath::Scale);

        Animation* c = model->create<Animation>();
        c->setName("C");
        addChannel(*c, *n2, AnimationPath::Translation);

        Animation* d = model->create<Animation>();
        d->setName("D");
        addChannel(*d, *n0, AnimationPath::Translation);

        Animation* e = model->create<Animation>();
        e->setName("E");
        addPointerChannel(*e, *n0, "translation");

        Animation* f = model->create<Animation>();
        f->setName("F");
        addPointerChannel(*f, *n0, "translation");
        addPointerChannel(*f, *n0, "translation");

        Animation* g = model->create<Animation>();
        g->setName("G");
        addPointerChannel(*g, *n1, "customProperty");

        Animation* h = model->create<Animation>();
        h->setName("H");
        addPointerChannel(*h, *n1, "customProperty");

        Animation* i = model->create<Animation>();
        i->setName("I");
        addNullChannel(*i, AnimationPath::Translation);

        Animation* j = model->create<Animation>();
        j->setName("J");
        addNullChannel(*j, AnimationPath::Translation);

        Animation* k = model->create<Animation>();
        k->setName("K");
        addNullChannel(*k, AnimationPath::Rotation);

        return model;
    };

    auto legacyGreedy = [this](Gltf& model) {
        auto& animations = model.getElements<Animation>();
        for (auto& animation : animations) {
            animation.setPlaying(false);
        }
        std::vector<bool> selected(animations.size(), false);
        std::vector<size_t> active;
        for (size_t i = 0; i < animations.size(); ++i) {
            bool canPlay = true;
            for (size_t activeIndex : active) {
                if (checkConflict(animations.at(i), animations.at(activeIndex))) {
                    canPlay = false;
                    break;
                }
            }
            selected[i] = canPlay;
            if (canPlay) {
                active.push_back(i);
            }
            animations.at(i).setPlaying(canPlay);
        }
        return selected;
    };

    auto legacyActive = [this](const Gltf& model, const std::vector<bool>& active) {
        const auto& animations = model.getElements<Animation>();
        std::vector<bool> result(animations.size(), false);
        for (size_t i = 0; i < animations.size(); ++i) {
            for (size_t j = 0; j < animations.size(); ++j) {
                if (i == j || !active[j]) {
                    continue;
                }
                if (checkConflict(animations.at(i), animations.at(j))) {
                    result[i] = true;
                    break;
                }
            }
        }
        return result;
    };

    auto currentActive = [](const Gltf& model) {
        const auto& animations = model.getElements<Animation>();
        std::vector<bool> result(animations.size(), false);
        for (size_t i = 0; i < animations.size(); ++i) {
            result[i] = animations.at(i).isPlaying();
        }
        return result;
    };

    auto indexedActive = [this](const Gltf& model) {
        const auto& animations = model.getElements<Animation>();
        std::vector<bool> result(animations.size(), false);
        for (size_t i = 0; i < animations.size(); ++i) {
            result[i] = isAnimationConflictingWithActive(i, model);
        }
        return result;
    };

    auto selectionVectorsEqual = [](const std::vector<bool>& a, const std::vector<bool>& b) {
        return a == b;
    };

    auto model = makeCaseModel();
    auto& animations = model->getElements<Animation>();
    check(animations.size() == 11, "case model clip count");
    check(animations.at(0).getChannels().size() == 2, "ordinary clip channel count");
    check(animations.at(1).getChannels().size() == 3, "duplicate-target clip channel count");
    check(animations.at(4).getChannels().front().getTarget().m_animationPointer.has_value(), "pointer target present");

    // Pairwise answers, including pointer, duplicate and null-target keys.
    size_t conflictingPairs = 0;
    size_t disjointPairs = 0;
    for (size_t i = 0; i < animations.size(); ++i) {
        for (size_t j = i + 1; j < animations.size(); ++j) {
            for (auto& animation : animations) {
                animation.setPlaying(false);
            }
            animations.at(j).setPlaying(true);
            rebuildActiveConflictCounts(*model);
            const bool indexed = isAnimationConflictingWithActive(i, *model);
            const bool legacy = checkConflict(animations.at(i), animations.at(j));
            check(indexed == legacy,
                "pair mismatch " + std::to_string(i) + "," + std::to_string(j)
                    + " indexed=" + (indexed ? "1" : "0") + " legacy=" + (legacy ? "1" : "0"));
            check(legacy == checkConflict(animations.at(j), animations.at(i)),
                "legacy pair symmetry " + std::to_string(i) + "," + std::to_string(j));
            if (legacy) {
                ++conflictingPairs;
            } else {
                ++disjointPairs;
            }
        }
    }
    check(conflictingPairs > 0, "ordinary/pointer duplicate conflicts covered");
    check(disjointPairs > 0, "disjoint clips covered");

    // Initial selection must be the same order-preserving greedy result.
    auto legacySelection = legacyGreedy(*model);
    selectAllPossibleAnimations(true, *model, nullptr);
    check(selectionVectorsEqual(currentActive(*model), legacySelection), "greedy selection differs from legacy");
    check(areAllPossibleAnimationsSelected(*model), "greedy selection is not maximal");

    selectAllPossibleAnimations(false, *model, nullptr);
    check(!areAllPossibleAnimationsSelected(*model), "empty active set should not report all selected");

    // Toggle sequences, including repeated on/off requests.
    selectAllPossibleAnimations(false, *model, nullptr);
    std::vector<bool> legacyState(animations.size(), false);
    std::vector<std::pair<size_t, bool>> toggles = {
        { 0, true }, { 1, true }, { 2, true }, { 3, true }, { 4, true }, { 5, true },
        { 0, false }, { 6, true }, { 7, true }, { 8, true }, { 9, true }, { 10, true },
        { 1, false }, { 4, false }, { 10, false }, { 5, true }, { 3, true }, { 0, true }
    };
    for (int step = 0; step < 120; ++step) {
        toggles.emplace_back(static_cast<size_t>((step * 7 + 3) % animations.size()), (step % 3) != 0);
    }

    for (const auto& [index, playing] : toggles) {
        if (playing) {
            bool canPlay = true;
            for (size_t activeIndex = 0; activeIndex < legacyState.size(); ++activeIndex) {
                if (activeIndex != index && legacyState[activeIndex]
                    && checkConflict(animations.at(index), animations.at(activeIndex))) {
                    canPlay = false;
                    break;
                }
            }
            if (canPlay) {
                legacyState[index] = true;
            }
        } else {
            legacyState[index] = false;
        }

        toggleAnimation(index, playing, *model, nullptr);
        const auto indexedState = currentActive(*model);
        check(selectionVectorsEqual(indexedState, legacyState),
            "toggle state mismatch at clip " + std::to_string(index));
        check(selectionVectorsEqual(indexedActive(*model), legacyActive(*model, legacyState)),
            "active query mismatch after toggle " + std::to_string(index));
    }

    // Playing flags can also change through undo/redo or JSON loading. A direct
    // setPlaying must invalidate the active-target counts without a player call.
    selectAllPossibleAnimations(false, *model, nullptr);
    animations.at(0).setPlaying(true);
    animations.at(1).setPlaying(true);
    animations.at(2).setPlaying(true);
    check(selectionVectorsEqual(indexedActive(*model), legacyActive(*model, currentActive(*model))),
        "external active-set change did not invalidate counts");

    // Channel mutation after the cache was built must rebuild the index.
    selectAllPossibleAnimations(false, *model, nullptr);
    animations.at(0).getChannels().front().getTarget().setNode(*animations.at(2).getChannels().front().getTarget().getNode());
    legacySelection = legacyGreedy(*model);
    selectAllPossibleAnimations(true, *model, nullptr);
    check(selectionVectorsEqual(currentActive(*model), legacySelection), "channel mutation did not invalidate index");
    for (size_t i = 0; i < animations.size(); ++i) {
        for (size_t j = i + 1; j < animations.size(); ++j) {
            for (auto& animation : animations) {
                animation.setPlaying(false);
            }
            animations.at(j).setPlaying(true);
            rebuildActiveConflictCounts(*model);
            check(isAnimationConflictingWithActive(i, *model) == checkConflict(animations.at(i), animations.at(j)),
                "channel mutation pair mismatch " + std::to_string(i) + "," + std::to_string(j));
        }
    }

    // Replacing the model must not retain clip targets from the previous model.
    auto replacement = std::make_unique<Gltf>();
    Node* r0 = replacement->create<Node>();
    Node* r1 = replacement->create<Node>();
    Animation* rA = replacement->create<Animation>();
    rA->setName("RA");
    auto* rAChannel = rA->createChannel();
    rAChannel->getTarget().setNode(*r0);
    rAChannel->getTarget().setPath(AnimationPath::Translation);
    Animation* rB = replacement->create<Animation>();
    rB->setName("RB");
    auto* rBChannel = rB->createChannel();
    rBChannel->getTarget().setNode(*r1);
    rBChannel->getTarget().setPath(AnimationPath::Rotation);
    Animation* rC = replacement->create<Animation>();
    rC->setName("RC");
    auto* rCChannel = rC->createChannel();
    rCChannel->getTarget().setNode(*r0);
    rCChannel->getTarget().setPath(AnimationPath::Translation);
    auto replacementLegacy = legacyGreedy(*replacement);
    setModel(*replacement, nullptr);
    check(selectionVectorsEqual(currentActive(*replacement), replacementLegacy), "model replacement retained stale targets");
    check(areAllPossibleAnimationsSelected(*replacement), "model replacement selection is not maximal");

    // Hundreds-of-clips benchmark. The legacy path is measured with the same
    // clip data and the same greedy order.
    auto benchmarkModel = std::make_unique<Gltf>();
    std::vector<Node*> benchmarkNodes;
    benchmarkNodes.reserve(50);
    for (int i = 0; i < 50; ++i) {
        Node* node = benchmarkModel->create<Node>();
        node->setName("BenchNode" + std::to_string(i));
        benchmarkNodes.push_back(node);
    }
    constexpr size_t benchmarkClipCount = 300;
    for (size_t i = 0; i < benchmarkClipCount; ++i) {
        Animation* animation = benchmarkModel->create<Animation>();
        animation->setName("BenchClip" + std::to_string(i));
        for (size_t channelIndex = 0; channelIndex < 4; ++channelIndex) {
            Node* node = benchmarkNodes[(i * 7 + channelIndex * 11 + channelIndex * channelIndex) % benchmarkNodes.size()];
            const AnimationPath path = channelIndex == 0 ? AnimationPath::Translation
                : channelIndex == 1 ? AnimationPath::Rotation
                : channelIndex == 2 ? AnimationPath::Scale
                : AnimationPath::Translation;
            auto* channel = animation->createChannel();
            channel->getTarget().setNode(*node);
            channel->getTarget().setPath(path);
        }
    }

    auto measureMs = [](int repetitions, const std::function<void()>& operation) {
        double bestMs = 1e100;
        for (int repetition = 0; repetition < repetitions; ++repetition) {
            const auto start = std::chrono::steady_clock::now();
            operation();
            const auto end = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
            bestMs = std::min(bestMs, elapsed);
        }
        return bestMs;
    };

    auto benchmarkLegacySelection = [this, &benchmarkModel, &legacyGreedy]() {
        legacyGreedy(*benchmarkModel);
    };
    auto benchmarkIndexSelection = [this, &benchmarkModel]() {
        selectAllPossibleAnimations(true, *benchmarkModel, nullptr);
    };

    auto benchmarkLegacy = legacyGreedy(*benchmarkModel);
    const double legacySelectionMs = measureMs(3, benchmarkLegacySelection);
    const double indexedSelectionMs = measureMs(100, benchmarkIndexSelection);
    selectAllPossibleAnimations(true, *benchmarkModel, nullptr);
    check(selectionVectorsEqual(currentActive(*benchmarkModel), benchmarkLegacy), "benchmark greedy selection differs");

    auto& benchmarkAnimations = benchmarkModel->getElements<Animation>();
    auto legacyRowScan = [this, &benchmarkAnimations]() {
        size_t conflicts = 0;
        for (size_t i = 0; i < benchmarkAnimations.size(); ++i) {
            for (size_t j = 0; j < benchmarkAnimations.size(); ++j) {
                if (i != j && benchmarkAnimations.at(j).isPlaying()
                    && checkConflict(benchmarkAnimations.at(i), benchmarkAnimations.at(j))) {
                    ++conflicts;
                    break;
                }
            }
        }
        return conflicts;
    };
    auto indexedRowScan = [this, &benchmarkModel, &benchmarkAnimations]() {
        size_t conflicts = 0;
        for (size_t i = 0; i < benchmarkAnimations.size(); ++i) {
            if (isAnimationConflictingWithActive(i, *benchmarkModel)) {
                ++conflicts;
            }
        }
        return conflicts;
    };

    const size_t legacyConflictRows = legacyRowScan();
    const size_t indexedConflictRows = indexedRowScan();
    check(legacyConflictRows == indexedConflictRows, "benchmark row conflict count differs");
    const double legacyRowsMs = measureMs(3, [&]() { (void)legacyRowScan(); });
    const double indexedRowsMs = measureMs(100, [&]() { (void)indexedRowScan(); });

    size_t selectedBenchmark = 0;
    for (const auto& animation : benchmarkAnimations) {
        if (animation.isPlaying()) {
            ++selectedBenchmark;
        }
    }

    if (forceFailure) {
        check(false, "forced conflict self-check failure");
    }

    std::ostringstream report;
    report << "P2.1 animation conflict self-check\n";
    report << "Result: " << (failures.empty() ? "PASS" : "FAIL") << "\n";
    report << "Checks: " << checks << "\n";
    report << "Coverage: ordinary, pointer, duplicate, disjoint, null-target, repeated toggles, direct-playing, channel/model invalidation\n";
    report << "Case clips: " << animations.size() << ", conflicting pairs: " << conflictingPairs
           << ", disjoint pairs: " << disjointPairs << "\n";
    report << "Benchmark clips: " << benchmarkClipCount << ", channels/clip: 4, selected: " << selectedBenchmark << "\n";
    report << std::fixed << std::setprecision(3);
    report << "Initial selection legacy: " << legacySelectionMs << " ms; indexed: " << indexedSelectionMs << " ms; speedup: "
           << (indexedSelectionMs > 0.0 ? legacySelectionMs / indexedSelectionMs : 0.0) << "x\n";
    report << "Per-row conflict scan legacy: " << legacyRowsMs << " ms; indexed: " << indexedRowsMs << " ms; speedup: "
           << (indexedRowsMs > 0.0 ? legacyRowsMs / indexedRowsMs : 0.0) << "x\n";
    report << "Conflicting rows: " << indexedConflictRows << "/" << benchmarkClipCount << "\n";
    for (const auto& failure : failures) {
        report << "FAIL: " << failure << "\n";
    }
    return SelfCheckResult{ failures.empty(), report.str() };
}

void AnimationPlayer::update(float deltaTime, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (m_isPlaying && m_maxDuration > 0.0f) {
        m_currentTime += deltaTime * m_playbackSpeed;
        if (m_currentTime > m_maxDuration) {
            if (m_isLooping) {
                if (m_loopCount > 0) {
                    m_currentLoop++;
                    if (m_currentLoop >= m_loopCount) {
                        m_currentTime = m_maxDuration;
                        m_isPlaying = false;
                        m_currentLoop = 0;
                    } else {
                        m_currentTime = std::fmod(m_currentTime, m_maxDuration);
                    }
                } else {
                    m_currentTime = std::fmod(m_currentTime, m_maxDuration);
                }
            } else {
                m_currentTime = m_maxDuration;
                m_isPlaying = false;
            }
        }
        if (gltfLayer != nullptr) {
            gltfLayer->applyAnimationTime(m_currentTime);
        }
    }
}

void AnimationPlayer::renderToolbar(ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer, bool& isSceneDirty)
{
    auto& anims = gltfModel.getElements<ux3d::gltf::Animation>();
    if (anims.empty()) {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.14f, 0.17f, 0.95f));
    ImGui::BeginChild("AnimationToolbar", ImVec2(0, 36), true, ImGuiWindowFlags_NoScrollbar);

    // 1. Animations Popup Menu Button (matching Gestaltor)
    size_t activeCount = 0;
    for (const auto& a : anims) {
        if (a.isPlaying()) ++activeCount;
    }
    char animBtnLabel[128];
    std::snprintf(animBtnLabel, sizeof(animBtnLabel), ICON_MDI_ANIMATION " Animations (%zu/%zu) ##Btn", activeCount, anims.size());
    if (ImGui::Button(animBtnLabel)) {
        ImGui::OpenPopup("AnimationsMenuPopup");
    }

    if (ImGui::BeginPopup("AnimationsMenuPopup")) {
        ImGui::TextDisabled("ANIMATIONS");
        ImGui::Separator();

        bool allSelected = areAllPossibleAnimationsSelected(gltfModel);
        if (ImGui::Checkbox("Select all non-conflicting", &allSelected)) {
            selectAllPossibleAnimations(allSelected, gltfModel, gltfLayer);
            isSceneDirty = true;
        }

        ImGui::Separator();

        float listHeight = std::min(260.0f, static_cast<float>(anims.size() * 26.0f));
        ImGui::BeginChild("AnimScrollList", ImVec2(300, listHeight), true);
        for (size_t i = 0; i < anims.size(); ++i) {
            auto& a = anims.at(i);
            bool isPlaying = a.isPlaying();
            bool conflict = !isPlaying && isAnimationConflictingWithActive(i, gltfModel);

            std::string name = a.hasName() ? a.getName().c_str() : ("Animation_" + std::to_string(i));

            if (conflict) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Checkbox((name + "##" + std::to_string(i)).c_str(), &isPlaying)) {
                toggleAnimation(i, isPlaying, gltfModel, gltfLayer);
                isSceneDirty = true;
            }
            if (conflict) {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("Conflicts with an already playing animation");
                }
            }
        }
        ImGui::EndChild();

        ImGui::TextDisabled("You can only select non-conflicting animations.");
        ImGui::Separator();

        ImGui::TextDisabled("SETTINGS");
        if (gltfLayer != nullptr) {
            if (ImGui::Checkbox("Skinning", &gltfLayer->renderSettings.enableSkinning)) {
                isSceneDirty = true;
            }
            if (ImGui::Checkbox("Morphing", &gltfLayer->renderSettings.enableMorphing)) {
                isSceneDirty = true;
            }
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // 2. Playback control buttons: |<, Play/Pause, Stop, >|
    if (ImGui::Button(ICON_MDI_SKIP_BACKWARD "##Rewind", ImVec2(34, 0))) {
        setCurrentTime(0.0f, gltfLayer);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rewind to start (0.0s)");

    ImGui::SameLine();
    if (m_isPlaying) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.15f, 1.0f));
        if (ImGui::Button(ICON_MDI_PAUSE " Pause", ImVec2(76, 0))) {
            pause();
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 1.0f));
        if (ImGui::Button(ICON_MDI_PLAY " Play", ImVec2(76, 0))) {
            play();
        }
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_STOP " Stop", ImVec2(68, 0))) {
        stop(gltfLayer);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop & Reset (0.0s)");

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_SKIP_FORWARD "##End", ImVec2(34, 0))) {
        setCurrentTime(m_maxDuration, gltfLayer);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Jump to end");

    ImGui::SameLine();
    // 3. Loop checkbox
    ImGui::Checkbox("Loop", &m_isLooping);

    ImGui::SameLine();
    // 4. Playback speed
    ImGui::SetNextItemWidth(72);
    const char* speedOptions[] = { "0.25x", "0.5x", "1.0x", "1.5x", "2.0x" };
    static int speedIdx = 2;
    if (ImGui::Combo("##Speed", &speedIdx, speedOptions, 5)) {
        if (speedIdx == 0) m_playbackSpeed = 0.25f;
        else if (speedIdx == 1) m_playbackSpeed = 0.5f;
        else if (speedIdx == 2) m_playbackSpeed = 1.0f;
        else if (speedIdx == 3) m_playbackSpeed = 1.5f;
        else if (speedIdx == 4) m_playbackSpeed = 2.0f;
    }

    ImGui::SameLine();
    // 5. Time formatted display
    char timeBuf[64];
    std::snprintf(timeBuf, sizeof(timeBuf), "%.2f / %.2fs", m_currentTime, m_maxDuration);
    ImGui::Text("%s", timeBuf);

    ImGui::SameLine();
    // 6. Timeline Scrubber slider
    float availW = ImGui::GetContentRegionAvail().x;
    if (availW > 40.0f) {
        ImGui::SetNextItemWidth(availW);
        if (m_maxDuration > 0.0f) {
            float t = m_currentTime;
            if (ImGui::SliderFloat("##Scrubber", &t, 0.0f, m_maxDuration, "%.2fs")) {
                m_currentTime = t;
                if (gltfLayer != nullptr) {
                    gltfLayer->applyAnimationTime(m_currentTime);
                }
            }
        } else {
            float dummy = 0.0f;
            ImGui::SliderFloat("##Scrubber", &dummy, 0.0f, 0.0f, "0.00s");
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace ux3d::imgui
