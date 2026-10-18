#include "model/GltfBinding.h"
#include "gltf/Gltf.h"
#include "gltf/animation/AnimationPath.h"
#include "model/ProjectManager.h"

using namespace ux3d;
using namespace ux3d::model;

GltfBinding::GltfBinding(IGltfPresentation* presentation)
    : TBinding<IGltfPresentation>(presentation)
{
    for (gltf::Animation& animation : ProjectManager::getInstance()->getGltf().getAnimations()) {
        m_animationBindings.emplace_back(&animation);
    }
    GltfEventManager::getInstance()->registerModel(*this);
}

GltfBinding::~GltfBinding()
{
    GltfEventManager::getInstance()->unregisterModel(*this);
}

void GltfBinding::refresh() const
{
    gltf::Asset& asset = ProjectManager::getInstance()->getGltf().getAsset();
    GltfEventManager::present(getID(), [&](auto& presenter) {
        presenter.presentAnimationSelectionList(collectSelectionList());
    });
}

void GltfBinding::disableAllAnimations()
{
    for (auto& animation : m_animationBindings) {
        animation.setPlaying(false);
    }

    ProjectManager::stateChangedTemporarily();

    GltfEventManager::present(getID(), [&](auto& presenter) {
        presenter.presentAnimationSelectionList(collectSelectionList());
    });
}

void GltfBinding::setAnimationPlaying(uint32_t _index, bool _isPlaying)
{
    if (_index < m_animationBindings.size()) {
        m_animationBindings[_index].setPlaying(_isPlaying);

        ProjectManager::stateChangedTemporarily();

        GltfEventManager::present(getID(), [&](auto& presenter) {
            presenter.presentAnimationSelectionList(collectSelectionList());
        });
    }
}

void GltfBinding::selectAllPossibleAnimations(bool _select)
{
    auto& animations = ProjectManager::getInstance()->getGltf().getAnimations();
    if (_select) {
        if (animations.size() > 0) {
            core::Vector<const gltf::Animation*> activeAnimations;
            animations.front().setPlaying(true);
            core::Vector<std::tuple<const gltf::Animation*, bool, bool>> result;

            activeAnimations.push_back(&animations.front());
            result.emplace_back(std::make_tuple(&animations.front(), true, true));

            for (auto animationIter = animations.begin() + 1; animationIter != animations.end(); ++animationIter) {
                gltf::Animation& animationToCheck = *animationIter;
                bool canPlay = true;
                for (const gltf::Animation* activeAnimation : activeAnimations) {
                    for (const gltf::AnimationChannel& checkChannel : animationToCheck.getChannels()) {
                        for (const gltf::AnimationChannel& activeChannel : activeAnimation->getChannels()) {

                            // gather animation targets that we have to check
                            std::string checkLocalPath = gltf::animationPathToString(checkChannel.getTarget().getPath());
                            const gltf::ChildOfRoot* checkChildOfRoot = checkChannel.getTarget().getNode();
                            if (checkChannel.getTarget().m_animationPointer.has_value()) {
                                // now we also have to consider animation path extension
                                checkLocalPath = std::get<1>(checkChannel.getTarget().m_animationPointer.value());
                                checkChildOfRoot = std::get<0>(checkChannel.getTarget().m_animationPointer.value());
                            }
                            // gather active animation targets
                            std::string activeLocalPath = gltf::animationPathToString(activeChannel.getTarget().getPath());
                            const gltf::ChildOfRoot* activeChildOfRoot = activeChannel.getTarget().getNode();
                            if (activeChannel.getTarget().m_animationPointer.has_value()) {
                                activeLocalPath = std::get<1>(activeChannel.getTarget().m_animationPointer.value());
                                activeChildOfRoot = std::get<0>(activeChannel.getTarget().m_animationPointer.value());
                            }

                            // compare active channel and channel to check against
                            if (checkChildOfRoot == activeChildOfRoot && checkLocalPath == activeLocalPath) {
                                canPlay = false;
                                break;
                            }
                        }
                        if (!canPlay) {
                            break;
                        }
                    }
                    if (!canPlay) {
                        break;
                    }
                }
                if (canPlay) {
                    activeAnimations.push_back(&animationToCheck);
                }
                animationToCheck.setPlaying(canPlay);
                result.emplace_back(std::make_tuple(&animationToCheck, canPlay, canPlay));
            }
            GltfEventManager::present(getID(), [&](auto& presenter) {
                presenter.presentAnimationSelectionList(result);
            });
        }
    } else {
        for (auto& animation : animations) {
            animation.setPlaying(false);
        }
        GltfEventManager::present(getID(), [&](auto& presenter) {
            presenter.presentAnimationSelectionList(collectSelectionList());
        });
    }
}

const core::Vector<std::tuple<const gltf::Animation*, bool, bool>> GltfBinding::collectSelectionList() const
{
    core::Vector<std::tuple<const gltf::Animation*, bool, bool>> result;
    core::Vector<const gltf::Animation*> activeAnimations;
    for (gltf::Animation& animation : ProjectManager::getInstance()->getGltf().getAnimations()) {
        if (animation.isPlaying()) {
            activeAnimations.push_back(&animation);
        }
    }
    for (gltf::Animation& animation : ProjectManager::getInstance()->getGltf().getAnimations()) {
        bool canPlay = true;
        for (const gltf::Animation* activeAnimation : activeAnimations) {
            for (const gltf::AnimationChannel& channel : animation.getChannels()) {
                for (const gltf::AnimationChannel& activeChannel : activeAnimation->getChannels()) {
                    if (&channel == &activeChannel) {
                        // do not compare with self
                        continue;
                    }

                    // compare regular animation targets and paths
                    if (channel.getTarget().getNode() != nullptr && channel.getTarget().getNode() == activeChannel.getTarget().getNode() && channel.getTarget().getPath() == activeChannel.getTarget().getPath()) {
                        canPlay = false;
                        break;
                    }

                    // compare KHR_animation_pointer node targets and pointers with active regular targets and paths
                    if (channel.getTarget().m_animationPointer.has_value()) {
                        auto [gltfObject, localPath] = channel.getTarget().m_animationPointer.value();
                        // check if target is equal and same property is animated
                        if (gltfObject == activeChannel.getTarget().getNode()) {
                            // these where moved into seperate if statements for improved readability
                            if (localPath == "translation" && activeChannel.getTarget().getPath() == gltf::AnimationPath::Translation) {
                                canPlay = false;
                                break;
                            }
                            if (localPath == "rotation" && activeChannel.getTarget().getPath() == gltf::AnimationPath::Rotation) {
                                canPlay = false;
                                break;
                            }
                            if (localPath == "scale" && activeChannel.getTarget().getPath() == gltf::AnimationPath::Scale) {
                                canPlay = false;
                                break;
                            }
                            if (localPath == "weights" && activeChannel.getTarget().getPath() == gltf::AnimationPath::Weights) {
                                canPlay = false;
                                break;
                            }
                        }
                    }

                    // compare active KHR_animation_pointer node targets and pointers with regular targets and paths
                    if (activeChannel.getTarget().m_animationPointer.has_value()) {
                        auto [gltfObject, localPath] = activeChannel.getTarget().m_animationPointer.value();
                        // check if target is equal and same property is animated
                        if (gltfObject == channel.getTarget().getNode()) {
                            // these where moved into seperate if statements for improved readability
                            if (localPath == "translation" && channel.getTarget().getPath() == gltf::AnimationPath::Translation) {
                                canPlay = false;
                                break;
                            }
                            if (localPath == "rotation" && channel.getTarget().getPath() == gltf::AnimationPath::Rotation) {
                                canPlay = false;
                                break;
                            }
                            if (localPath == "scale" && channel.getTarget().getPath() == gltf::AnimationPath::Scale) {
                                canPlay = false;
                                break;
                            }
                            if (localPath == "weights" && channel.getTarget().getPath() == gltf::AnimationPath::Weights) {
                                canPlay = false;
                                break;
                            }
                        }
                    }

                    // compare KHR_animation_pointer node targets and pointers with each other
                    if (activeChannel.getTarget().m_animationPointer.has_value() && channel.getTarget().m_animationPointer.has_value()) {
                        auto [activeGltfObject, activeLocalPath] = activeChannel.getTarget().m_animationPointer.value();
                        auto [gltfObject, localPath] = channel.getTarget().m_animationPointer.value();

                        if (activeGltfObject == gltfObject) {
                            if (activeLocalPath == localPath) {
                                canPlay = false;
                                break;
                            }
                        }
                    }
                }
                if (!canPlay) {
                    break;
                }
            }
            if (!canPlay) {
                break;
            }
        }
        result.emplace_back(std::make_tuple(&animation, animation.isPlaying(), canPlay));
    }

    return result;
}

bool GltfBinding::arePlayingAnimationsConflicting()
{
    auto animationList = collectSelectionList();
    for (auto& animationTuple : animationList) {
        // check whether an animation is currently playing (tuple 1) that is forbidden from being played (tuple 2)
        if (std::get<1>(animationTuple) && std::get<2>(animationTuple) == false) {
            return true;
        }
    }

    return false;
}

GltfEventManager* GltfEventManager::getInstance()
{
    static GltfEventManager instance;
    return &instance;
}
