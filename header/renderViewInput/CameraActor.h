#pragma once

#include "math/VectorTypes.h"
#include "renderer/Camera.h"
#include <limits>
#include <vector>

namespace ux3d::gltf {
class Scene;
class Node;
class Gltf;
struct Extends;
}

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::editor {

enum class CameraOrientation {
    Top,
    Bottom,
    Right,
    Left,
    Front,
    Back,
    Isometric
};

class CameraActor {
public:
    CameraActor() = default;

    void setCamera(renderer::Camera* _camera);
    void changeUserCameraType(renderer::Camera::Type _type);
    renderer::Camera* getUserCamera() const;

    void presentFocusNode(const renderer::GltfRenderLayer* _scene, const gltf::Node* _node, const gltf::Gltf* _gltf, float margin = 1.0f);
    void changeCamSpeed(float _increment);
    // Frames the whole scene so its bounding box fills the viewport. Without _keepRotation it also
    // turns to the isometric home view (front-left, above).
    void fitCameraToScene(const gltf::Scene& _scene, const gltf::Gltf* _gltf, bool _keepRotation = false);
    // Near (and the zoom limits) follow extents at the current zoom. With _reach, the far plane also keeps every
    // corner of _reach in range from the current eye (focus on a small node keeps the rest of the scene visible).
    void updateClippingPlanes(const gltf::Extends& extents, const gltf::Extends* _reach = nullptr);
    void updateUserCameraTransform();
    void rotateCamera(int32_t _deltaX, int32_t _deltaY);
    void setCameraRotation(float _x, float _y);
    void setCameraEyeAndTarget(const math::vec3& eye, const math::vec3& target);
    void orientUserCamera(CameraOrientation orientation);
    void panCamera(int32_t _deltaX, int32_t _deltaY);
    void zoomCamera(float _delta);
    void applyOrbitCamOrientationToFreeCam();
    void applyFreeCamOrientationToOrbitCam();
    void handleFreeCamera(int32_t _mouseDeltaX, int32_t _mouseDeltaY, const math::vec3& _velocity);
    //Does not update the transform by itself
    void setZoom(float _zoom);
    //Does not update the transform by itself
    void setTarget(math::vec3 target) { m_lookTarget = target; }

    math::vec3 getEyePosition() const;
    math::vec3 getLookTarget() const { return m_lookTarget; }
    float getZoom() const { return m_zoom; }
    float getRotationX() const { return m_rotationX; }
    float getRotationY() const { return m_rotationY; }

private:
    math::vec3 toLocalRotation(const math::vec3& _vector) const;
    void updateCameraSpeeds(gltf::Extends& _extends, const float _yFov, const float _aspectRatio);
    // Sets target and distance so the points (or, without points, the box corners) fill the view at the current
    // rotation, FOV and aspect.
    void fitPointsToView(const gltf::Extends& _extends, const std::vector<math::dvec3>& _points);

    // Isometric home view: 45 degrees round from the front, true isometric elevation atan(1/sqrt(2)).
    static constexpr float isometricYaw = 0.78539816f;
    static constexpr float isometricPitch = 0.61547971f;

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
