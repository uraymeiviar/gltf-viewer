// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Animation.h"
#include "gltf/ChildOfRoot.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/Node.h"
#include "gltf/json/PointerLookup.h"
#include <string>
#include <vector>

using namespace ux3d;
using namespace ux3d::gltf;

ux3d::gltf::GltfResult ux3d::gltf::AnimationChannelTarget::setNode(Node& _Node)
{
    m_pNode = &_Node;
    m_animationHasChanged = true;
    bumpTargetRevision();
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::AnimationChannelTarget::setPath(AnimationPath _Path)
{
    m_Path = _Path;
    m_animationHasChanged = true;
    bumpTargetRevision();
    return GltfResult::Success;
}

bool ux3d::gltf::AnimationChannelTarget::operator==(const AnimationChannelTarget& other) const
{
    if (other.m_animationPointer.has_value() && m_animationPointer.has_value()) {
        auto [otherObject, otherPath] = other.m_animationPointer.value();
        auto [thisObject, thisPath] = m_animationPointer.value();

        return thisObject == otherObject
            && otherPath == thisPath;
    }

    return m_Path == other.m_Path
        && m_pNode == other.m_pNode;
}

std::string AnimationChannelTarget::toAnimationPointer(const IndexLookup& indexLookup, std::tuple<ChildOfRoot*, std::string> animationTuple)
{
    auto [gltfObject, localPath] = animationTuple;
    std::string global = "/";
    size_t index = 0;
    if (gltfObject == nullptr) {
        return "";
    }
    if (gltfObject->getGltfType() == GltfType::Node) {
        index = indexLookup[static_cast<Node*>(gltfObject)];
        global += "nodes";
    } else if (gltfObject->getGltfType() == GltfType::Camera) {
        index = indexLookup[static_cast<Camera*>(gltfObject)];
        global += "cameras";
    } else if (gltfObject->getGltfType() == GltfType::Material) {
        index = indexLookup[static_cast<Material*>(gltfObject)];
        global += "materials";
    } else if (gltfObject->getGltfType() == GltfType::Mesh) {
        index = indexLookup[static_cast<Mesh*>(gltfObject)];
        global += "meshes";
    } else if (gltfObject->getGltfType() == GltfType::Light) {
        index = indexLookup[static_cast<Light*>(gltfObject)];
        global += "extensions/KHR_lights_punctual/lights";
    }

    return global + "/" + std::to_string(index) + "/" + localPath;
}

std::tuple<ChildOfRoot*, std::string> AnimationChannelTarget::fromAnimationPointer(PointerLookup& pointerLookup, std::string globalPointer)
{
    auto splitString = [](std::string s, const std::string& delimiter) -> std::vector<std::string> {
        std::vector<std::string> output;
        size_t pos = 0;
        std::string token;
        while ((pos = s.find(delimiter)) != std::string::npos) {
            token = s.substr(0, pos);
            output.push_back(token);
            s.erase(0, pos + delimiter.length());
        }
        output.push_back(s);
        return output;
    };
    auto tokens = splitString(globalPointer, "/");
    if (tokens.size() <= 3) { // At least: empty, category, objectID, localPath
        core::logWarning("Invalid token count: ");
        for (auto token : tokens) {
            core::logInfo("-> %s", token.c_str());
        }
        return std::tuple<ChildOfRoot*, std::string>(nullptr, "");
    }
    ChildOfRoot* gltfObject = nullptr;
    std::string localPath = "";

    if (tokens[1] == "nodes") {
        uint32_t objectID = static_cast<uint32_t>(std::stoi(tokens[2]));
        if (objectID >= pointerLookup.Nodes.size()) {
            core::logError("Invalid animation target pointer %s", globalPointer.data());
        }
        gltfObject = pointerLookup.Nodes[objectID];
        localPath = tokens[3];
    } else if (tokens[1] == "cameras") {
        uint32_t objectID = static_cast<uint32_t>(std::stoi(tokens[2]));
        if (objectID >= pointerLookup.Cameras.size()) {
            core::logError("Invalid animation target pointer %s", globalPointer.data());
        }
        gltfObject = pointerLookup.Cameras[objectID];
        localPath = tokens[3];
        for (size_t i = 4; i < tokens.size(); i++) {
            localPath += "/" + tokens[i];
        }
    } else if (tokens[1] == "materials") {
        uint32_t objectID = static_cast<uint32_t>(std::stoi(tokens[2]));
        if (objectID >= pointerLookup.Materials.size()) {
            core::logError("Invalid animation target pointer %s", globalPointer.data());
        }
        gltfObject = pointerLookup.Materials[objectID];
        localPath = tokens[3]; // E.g. alphaCutoff
        for (size_t i = 4; i < tokens.size(); i++) {
            localPath += "/" + tokens[i]; // E.g. normalTexture/scale, extensions/KHR_materials_emissive_strength/emissiveStrength
        }
    } else if (tokens[1] == "meshes") {
        uint32_t objectID = static_cast<uint32_t>(std::stoi(tokens[2]));
        if (objectID >= pointerLookup.Meshes.size()) {
            core::logError("Invalid animation target pointer %s", globalPointer.data());
        }
        gltfObject = pointerLookup.Meshes[objectID];
        localPath = tokens[3]; // weights
    } else if (tokens.size() >= 5 && tokens[1] == "extensions" && tokens[2] == "KHR_lights_punctual" && tokens[3] == "lights") {
        uint32_t objectID = static_cast<uint32_t>(std::stoi(tokens[4]));
        if (objectID >= pointerLookup.Lights.size()) {
            core::logError("Invalid animation target pointer %s", globalPointer.data());
        }
        gltfObject = pointerLookup.Lights[objectID];
        localPath = tokens[5]; // E.g. intensity
        for (size_t i = 6; i < tokens.size(); i++) {
            localPath += "/" + tokens[i];
        }
    }

    if (localPath.length() == 0) {
        core::logWarning("Unknown tokens: ");
        for (auto token : tokens) {
            core::logInfo("-> %s", token.c_str());
        }
    }

    return std::tuple<ChildOfRoot*, std::string>(gltfObject, localPath);
}
