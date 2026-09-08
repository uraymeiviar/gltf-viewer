#pragma once

#include "core/Vector.h"
#include "model/AnimationBinding.h"
#include "model/ReportBehavior.h"
#include "model/TBinding.h"
#include "model/TEventManager.h"
#include <string>
#include <tuple>

namespace ux3d::gltf {
class Animation;
class Gltf;
class Packet;
}

namespace ux3d::model {

class IGltfPresentation {
public:
    virtual ~IGltfPresentation() = default;
    virtual void presentAnimationSelectionList(const core::Vector<std::tuple<const gltf::Animation*, bool, bool>> _animationTuples) { }
};

class GltfBinding : public TBinding<IGltfPresentation> {
public:
    using TBinding::ID;

    ID getID() const override { return 0; };

    GltfBinding(IGltfPresentation* presentation = nullptr);
    ~GltfBinding() override;

    void refresh() const;
    void disableAllAnimations();
    void setAnimationPlaying(uint32_t _index, bool _isPlaying);
    void selectAllPossibleAnimations(bool _select = true);
    bool arePlayingAnimationsConflicting();

private:
    // create map of non conflicting animations
    const core::Vector<std::tuple<const gltf::Animation*, bool, bool>> collectSelectionList() const;
    // Only use this for the AnimationPopup until all bindings are migrated!
    core::Vector<AnimationBinding> m_animationBindings;
};

class GltfEventManager : public TEventManager<GltfEventManager, GltfBinding, IGltfPresentation> {
public:
    static GltfEventManager* getInstance();
};
}
