// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "AnimationPath.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include <optional>
#include <string>

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#target

namespace ux3d::gltf {
class Animation;
class Node;
struct PointerLookup;
class ChildOfRoot;
class IndexLookup;

class AnimationChannelTarget : public GltfObject {
private:
    Node* m_pNode = nullptr;
    AnimationPath m_Path = AnimationPath::Translation;

public:
    const Animation& m_RootAnimation;

    AnimationChannelTarget(const AnimationChannelTarget&) = delete;

    AnimationChannelTarget(AnimationChannelTarget&&) = default;
    virtual ~AnimationChannelTarget() = default;

    explicit AnimationChannelTarget(const Animation& _RootAnimation)
        : m_RootAnimation(_RootAnimation)
    {
    }

    AnimationChannelTarget(const Animation& _rootAnimation, const AnimationChannelTarget& _source)
        : GltfObject(_source)
        , m_Path(_source.m_Path)
        , m_RootAnimation(_rootAnimation)
    {
    }

    Node* getNode() { return m_pNode; }
    const Node* getNode() const { return m_pNode; }

    AnimationPath getPath() const { return m_Path; }

    GltfResult setNode(Node& _Node);
    void unsetNode() { m_pNode = nullptr; }
    GltfResult setPath(AnimationPath _Path);
    bool m_animationHasChanged = false;

    static std::tuple<ChildOfRoot*, std::string> fromAnimationPointer(PointerLookup& pointerLookup, std::string globalPointer);
    static std::string toAnimationPointer(const IndexLookup& indexLookup, std::tuple<ChildOfRoot*, std::string> animationPointerTuple);

    std::optional<std::tuple<ChildOfRoot*, std::string>> m_animationPointer = std::nullopt; // e.g. Material, Node + color, intensity, rotation

    bool operator==(const AnimationChannelTarget& other) const;

    // When node isn't defined, channel should be ignored
    bool isValid() const { return true; }
    bool isIgnored() const { return m_pNode == nullptr; }
};
}
