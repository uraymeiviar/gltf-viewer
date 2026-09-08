// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "renderer/gltf/GltfLightMapping.h"
#include "gltf/Light.h"
#include "renderer/KHRAnimationPointerTarget.h"
#include "renderer/Lights.h"
#include "renderer/TransformHierarchy.h"
#include "renderer/gltf/GltfRenderLayer.h"

using namespace ux3d;
using namespace ux3d::renderer;

GltfLightMapping::GltfLightMapping(GltfRenderLayer& _scene)
    : GltfMapping<GltfLightTuple, RendererLightTuple>(_scene)
{
}

bool GltfLightMapping::onUpdateMapping(GltfLightTuple _gltfTuple, RendererLightTuple _lightTuple)
{
    auto [glTFNode, pGltfLight] = _gltfTuple;
    auto [light] = _lightTuple;

    math::vec3 lightColor = pGltfLight->getColorOrDefault();
    float max = math::max(lightColor.r, math::max(lightColor.g, lightColor.b));
    lightColor *= 1.0f / max;

    light->color = static_cast<math::vec3>(pGltfLight->getColorOrDefault());
    light->intensity = static_cast<float>(pGltfLight->getIntensityOrDefault());

    switch (pGltfLight->getType()) {
    case gltf::Light::Type::Directional:
        light->type = renderer::Light::Type::Directional;
        break;
    case gltf::Light::Type::Point:
        light->type = renderer::Light::Type::Point;
        light->range = static_cast<float>(pGltfLight->getRangeOrDefault());
        break;
    case gltf::Light::Type::Spot:
        light->type = renderer::Light::Type::Spot;
        light->range = static_cast<float>(pGltfLight->getRangeOrDefault());
        light->innerConeCos = static_cast<float>(math::cos(pGltfLight->getSpot()->getInnerConeAngleOrDefault()));
        light->outerConeCos = static_cast<float>(math::cos(pGltfLight->getSpot()->getOuterConeAngleOrDefault()));
        break;
    default:
        break;
    }
    return true;
}

RendererLightTuple GltfLightMapping::onCreateMapping(GltfLightTuple gltfTuple)
{
    auto [gltfNode, gltfLight] = gltfTuple;

    auto light = new Light;
    light->type = renderer::Light::Type::Invalid;

    RendererLightTuple tuple = { light };
    onUpdateMapping(gltfTuple, tuple);

    // KHR animation pointer
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfLight, "intensity" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [light](const float& value) -> void {
            light->intensity = value;
        }
    };
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfLight, "color" }].callback = KHRAnimationPointerTarget::Callback<glm::vec3> {
        [light](const glm::vec3& value) -> void {
            light->color = value;
        }
    };
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfLight, "range" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [light](const float& value) -> void {
            light->range = value;
        }
    };
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfLight, "spot/innerConeAngle" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [light](const float& value) -> void {
            light->innerConeCos = static_cast<float>(math::cos(value));
        }
    };
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfLight, "spot/outerConeAngle" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [light](const float& value) -> void {
            light->outerConeCos = static_cast<float>(math::cos(value));
        }
    };

    auto nodeIt = m_gltfRenderLayer.m_transformHierarchyMapping.map.find(gltfNode);
    if (nodeIt != m_gltfRenderLayer.m_transformHierarchyMapping.map.end()) {
        nodeIt->second->addLight(light);
    }

    return tuple;
}

void GltfLightMapping::onDestroyMapping(GltfLightTuple _gltfTuple, RendererLightTuple _lightTuple)
{
    auto [glTFNode, pGltfLight] = _gltfTuple;
    auto [light] = _lightTuple;

    auto nodeIt = m_gltfRenderLayer.m_transformHierarchyMapping.map.find(glTFNode);
    if (nodeIt != m_gltfRenderLayer.m_transformHierarchyMapping.map.end()) {
        nodeIt->second->removeLight(light);
    }

    const gltf::ChildOfRoot* gltfObject = pGltfLight;

    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "intensity" });
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "color" });
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "range" });
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "spot/innerConeAngle" });
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "spot/outerConeAngle" });

    delete light;
}
