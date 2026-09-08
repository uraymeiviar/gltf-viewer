// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "renderer/gltf/GltfCameraMapping.h"
#include "imageformats/imageformats.h"
#include "renderer/Camera.h"
#include "renderer/TransformHierarchy.h"
#include "renderer/gltf/GltfRenderLayer.h"

#include <algorithm>

using namespace ux3d;
using namespace ux3d::renderer;

GltfCameraMapping::GltfCameraMapping(GltfRenderLayer& scene)
    : GltfMapping<GltfCameraTuple, RendererCameraTuple>(scene)
{
}

bool GltfCameraMapping::onUpdateMapping(ux3d::renderer::GltfCameraTuple gltfTuple, ux3d::renderer::RendererCameraTuple rendererTuple)
{
    auto& [glTFNode, gltfCam] = gltfTuple;
    auto& [camera] = rendererTuple;

    // Projection
    switch (gltfCam->getType()) {
    case gltf::Camera::ProjectionType::Orthographic: {
        const auto& inCam = gltfCam->getCameraOrthographic();

        if (inCam.has_value()) {
            camera->setOrthographic(
                static_cast<float>(inCam->getXMag()),
                static_cast<float>(inCam->getYMag()),
                static_cast<float>(inCam->getZNear()),
                static_cast<float>(inCam->getZFar()));
        }
    } break;
    case gltf::Camera::ProjectionType::Perspective: {
        const auto& inCam = gltfCam->getCameraPerspective();
        if (inCam.has_value()) {
            camera->setPerspective(
                static_cast<float>(inCam->getAspectRatio().value_or(m_gltfRenderLayer.viewportAspectRatio)),
                static_cast<float>(inCam->getYFov()),
                static_cast<float>(inCam->getZNear()),
                static_cast<float>(inCam->getZFar().value_or(-1.)));
        }
    } break;
    default:
        break;
    }
    return true;
}

ux3d::renderer::RendererCameraTuple GltfCameraMapping::onCreateMapping(ux3d::renderer::GltfCameraTuple gltfTuple)
{
    auto& [gltfNode, gltfCamera] = gltfTuple;

    auto camera = new Camera { m_gltfRenderLayer.m_resourceManager.m_gl };

    auto nodeIt = m_gltfRenderLayer.m_transformHierarchyMapping.map.find(gltfNode);
    if (nodeIt != m_gltfRenderLayer.m_transformHierarchyMapping.map.end()) {
        nodeIt->second->addCamera(camera);
    }

    // KHR_animation_pointer
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfCamera, "perspective/aspectRatio" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [camera](const float& value) -> void {
            camera->setAspectRatio(value);
        }
    };
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfCamera, "perspective/yfov" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [camera](const float& value) -> void {
            camera->setPerspective(camera->m_aspectRatio, value, camera->m_zNear, camera->m_zFar);
        }
    };
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfCamera, "perspective/znear" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [camera](const float& value) -> void {
            camera->setPerspective(camera->m_aspectRatio, camera->m_yFoV, value, camera->m_zFar);
        }
    };
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfCamera, "perspective/zfar" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [camera](const float& value) -> void {
            camera->setPerspective(camera->m_aspectRatio, camera->m_yFoV, camera->m_zNear, value);
        }
    };
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfCamera, "orthographic/xmag" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [camera](const float& value) -> void {
            camera->setOrthographic(value, camera->m_yMag, camera->m_zNear, camera->m_zFar);
        }
    };
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfCamera, "orthographic/ymag" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [camera](const float& value) -> void {
            camera->setOrthographic(camera->m_xMag, value, camera->m_zNear, camera->m_zFar);
        }
    };
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfCamera, "orthographic/znear" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [camera](const float& value) -> void {
            camera->setOrthographic(camera->m_xMag, camera->m_yMag, value, camera->m_zFar);
        }
    };
    m_gltfRenderLayer.m_animationPointerTargets[{ gltfCamera, "orthographic/zfar" }].callback = KHRAnimationPointerTarget::Callback<float> {
        [camera](const float& value) -> void {
            camera->setOrthographic(camera->m_xMag, camera->m_yMag, camera->m_zNear, value);
        }
    };

    RendererCameraTuple rendererTuple = { camera };
    onUpdateMapping(gltfTuple, rendererTuple);
    return rendererTuple;
}

void GltfCameraMapping::onDestroyMapping(ux3d::renderer::GltfCameraTuple gltfTuple, ux3d::renderer::RendererCameraTuple rendererTuple)
{
    auto& [glTFNode, gltfCam] = gltfTuple;
    auto [camera] = rendererTuple;

    auto nodeIt = m_gltfRenderLayer.m_transformHierarchyMapping.map.find(glTFNode);
    if (nodeIt != m_gltfRenderLayer.m_transformHierarchyMapping.map.end()) {
        nodeIt->second->removeCamera(camera);
    }

    if (m_gltfRenderLayer.m_activeCamera == camera) {
        m_gltfRenderLayer.m_activeCamera = nullptr;
    }

    const gltf::ChildOfRoot* gltfObject = gltfCam;

    // erase animation pointer for perspective
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "perspective/aspectRatio" });
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "perspective/yfov" });
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "perspective/znear" });
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "perspective/zfar" });

    // erase animation pointer for orthographic
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "orthographic/xmag" });
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "orthographic/ymag" });
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "orthographic/znear" });
    m_gltfRenderLayer.m_animationPointerTargets.erase({ gltfObject, "orthographic/zfar" });

    delete camera;
}
