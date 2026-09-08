#pragma once

#include "math/VectorTypes.h"
#include "renderer/Camera.h"
#include <limits>

namespace ux3d::gltf {
class Scene;
class Node;
struct Extends;
}

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::editor {
class CameraActor {
public:
    CameraActor() = default;

    void setCamera(renderer::Camera* _camera);
    void changeUserCameraType(renderer::Camera::Type _type);
    renderer::Camera* getUserCamera() const;

    void presentFocusNode(const renderer::GltfRenderLayer* _scene, const gltf::Node* _node);
    void changeCamSpeed(float _increment);
    void fitCameraToScene(const gltf::Scene& _scene);
    void updateClippingPlanes(const gltf::Extends& extents);
    void updateUserCameraTransform();
    void rotateCamera(int32_t _deltaX, int32_t _deltaY);
    void setCameraRotation(float _x, float _y);
    void panCamera(int32_t _deltaX, int32_t _deltaY);
    void zoomCamera(float _delta);
    void applyOrbitCamOrientationToFreeCam();
    void applyFreeCamOrientationToOrbitCam();
    void handleFreeCamera(int32_t _mouseDeltaX, int32_t _mouseDeltaY, const math::vec3& _velocity);
    //Does not update the transform by itself
    void setZoom(float _zoom);
    //Does not update the transform by itself
    void setTarget(math::vec3 target) { m_lookTarget = target; }

private:
    math::vec3 toLocalRotation(const math::vec3& _vector);
    void updateCameraSpeeds(gltf::Extends& _extends, const float _yFov, const float _aspectRatio);

    math::vec3 m_lookTarget = math::vec3(0.f, 0.f, 0.f);
    float m_lastCamPitch = 0;
    float m_lastCamYaw = 0;
    renderer::Camera* m_pUserCamera = nullptr;

    float m_rotationX = 0.f;
    float m_rotationY = 0.f;

    float m_zoom = 1.f;
    float m_zoomBase = 1.f;
    float m_zoomSpeed = 0.1f;
    float m_zoomMax = std::numeric_limits<float>::infinity();
    static constexpr float zoomMin = 0.0000001f; // zoom should not be zero, however numeric_limits<float>::min() does not produce valid results
    float m_zoomMin = zoomMin;
    float m_panSpeed = 1.f;
    float m_panSpeedBase = 1.f;
    float m_freeCamSpeed = 0.5f;
    float m_freeCamSpeedBase = 0.5f;
};
}
