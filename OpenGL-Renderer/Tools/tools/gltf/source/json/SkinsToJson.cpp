// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/SkinsToJson.h"
#include "gltf/IndexLookup.h"
#include "gltf/Skin.h"

void ux3d::gltf::SkinsToJson::convert(const Skin& _skin, nlohmann::json& _jsonSkin, const JsonPath& _jsonPath) const
{
    convertBase(_skin, _jsonSkin);
    convertInverseBindMatrices(_skin, _jsonSkin);
    convertSkeleton(_skin, _jsonSkin);
    convertJoints(_skin, _jsonSkin);
}

void ux3d::gltf::SkinsToJson::convertInverseBindMatrices(const Skin& _skin, nlohmann::json& _jsonSkin) const
{
    if (_skin.getInverseBindMatricesAccessor() == nullptr) {
        return;
    }

    size_t index = m_indices[_skin.getInverseBindMatricesAccessor()];
    _jsonSkin["inverseBindMatrices"] = index;
}

void ux3d::gltf::SkinsToJson::convertSkeleton(const Skin& _skin, nlohmann::json& _jsonSkin) const
{
    if (_skin.getSkeletonRoot() == nullptr) {
        return;
    }

    size_t index = m_indices[_skin.getSkeletonRoot()];
    _jsonSkin["skeleton"] = index;
}

void ux3d::gltf::SkinsToJson::convertJoints(const Skin& _skin, nlohmann::json& _jsonSkin) const
{
    if (_skin.getJoints().empty()) {
        return;
    }

    nlohmann::json jsonJoints = nlohmann::json::array();
    for (const Node* pJoint : _skin.getJoints()) {
        size_t index = m_indices[pJoint];
        jsonJoints.emplace_back(index);
    }

    _jsonSkin["joints"] = jsonJoints;
}
