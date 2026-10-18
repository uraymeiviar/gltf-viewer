#include "renderViewInput/CameraActor.h"
#include "gltf/Scene.h"
#include "gltf/Utils.h"
#include "math/VectorFunctions.h"
#include "math/VectorTypes.h"
#include "renderer/gltf/GltfRenderLayer.h"

using namespace ux3d;
using namespace ux3d::editor;

void CameraActor::setCamera(renderer::Camera* _camera)
{
    m_pUserCamera = _camera;
}

void CameraActor::setZoom(float _zoom)
{
    m_zoom = std::max(m_zoomMin, std::min(m_zoomMax, _zoom));

    if (m_pUserCamera != nullptr) {
        m_pUserCamera->setZoom(_zoom);
    }
}

void CameraActor::changeUserCameraType(renderer::Camera::Type _type)
{
    if (m_pUserCamera) {
        m_pUserCamera->changeType(_type, m_zoom);
    }
}

renderer::Camera* CameraActor::getUserCamera() const
{
    return m_pUserCamera;
}

void CameraActor::changeCamSpeed(float _increment)
{
    m_freeCamSpeed *= 1.f + _increment;
}

void CameraActor::presentFocusNode(const renderer::GltfRenderLayer* renderLayer, const gltf::Node* _node, const gltf::Gltf* _gltf)
{
    if (_node == nullptr || _gltf == nullptr) {
        return;
    }
    constexpr float aspectRatio = 1.f;
    constexpr float yFov = math::radians(45.f);

    math::dmat4 parentTransform = (renderLayer != nullptr) ? math::dmat4(renderLayer->getParentWorldTransform(_node)) : math::identity<math::dmat4>();
    gltf::Extends extends = gltf::utils::getExtends({ const_cast<gltf::Node*>(_node) }, _gltf, parentTransform);

    // if extends are singular we need to assume some reasonable extends
    if (extends.min == extends.max) {
        extends.min = extends.center - math::dvec3(1, 1, 1);
        extends.max = extends.center + math::dvec3(1, 1, 1);
    }

    m_zoom = static_cast<float>(gltf::utils::getFittingCameraZoom(extends, yFov, aspectRatio));
    updateClippingPlanes(extends);

    m_lookTarget = extends.center;
    updateUserCameraTransform();

    updateCameraSpeeds(extends, yFov, aspectRatio);

    if (m_pUserCamera != nullptr && m_pUserCamera->getType() == renderer::Camera::Type::Orthographic) {
        auto* camera = m_pUserCamera;
        float ratio = atan(camera->getYFoV() / 2.0f);
        camera->setOrthographic(camera->getAspectRatio() * m_zoom * ratio, m_zoom * ratio);
    }
}

void CameraActor::fitCameraToScene(const gltf::Scene& _scene, const gltf::Gltf* _gltf)
{
    if (m_pUserCamera == nullptr || _gltf == nullptr) {
        return;
    }

    // Gestaltor standard initial rotation: looking at front
    m_rotationX = 0.f;
    m_rotationY = 0.f;

    constexpr float aspectRatio = 1.f;
    constexpr float yFov = math::radians(45.f);

    gltf::Extends extends = gltf::utils::getExtends(_scene.getNodes(), _gltf);

    if (math::distance(extends.max, extends.min) <= 0.00001f) {
        extends.min = extends.center - math::dvec3(1, 1, 1);
        extends.max = extends.center + math::dvec3(1, 1, 1);
    }

    m_zoomMin = zoomMin;
    m_zoomMax = std::numeric_limits<float>::infinity();
    updateCameraSpeeds(extends, yFov, aspectRatio);
    m_lookTarget = extends.center;
    updateUserCameraTransform();

    updateClippingPlanes(extends);
}

void CameraActor::updateClippingPlanes(const gltf::Extends& extents)
{
    if (m_pUserCamera == nullptr) {
        return;
    }
    gltf::ClippingPlanes clippingPlanes = gltf::utils::getFittingClippingPlanes(extents, m_zoom);
    clippingPlanes.zNear = math::max(clippingPlanes.zFar, 0.0); // should not be negative
    clippingPlanes.zFar = math::abs(clippingPlanes.zFar);

    // the user might be zooming around so we move the clipping planes further apart
    constexpr float clippingPlanesScale = 100.f;

    // limit the zooming to the clipping planes
    m_zoomMax = static_cast<float>(clippingPlanes.zFar) * clippingPlanesScale * 0.5f; // 0.5 ensures most of the grid is still visible
    m_zoomMin = static_cast<float>(clippingPlanes.zNear) / clippingPlanesScale * 1.1f;

    const float ratio = atan(m_pUserCamera->getYFoV() / 2.0f);
    if (m_pUserCamera->getType() == renderer::Camera::Type::Orthographic) {
        m_pUserCamera->setOrthographic(m_pUserCamera->getAspectRatio() * m_zoom * ratio, m_zoom * ratio, static_cast<float>(clippingPlanes.zNear) / clippingPlanesScale,
            static_cast<float>(clippingPlanes.zFar) * clippingPlanesScale);
    } else {
        m_pUserCamera->setPerspective(m_pUserCamera->getAspectRatio(), m_pUserCamera->getYFoV(),
            static_cast<float>(clippingPlanes.zNear) / clippingPlanesScale,
            static_cast<float>(clippingPlanes.zFar) * clippingPlanesScale);
    }
}

void CameraActor::updateCameraSpeeds(gltf::Extends& _extends, const float _yFov, const float _aspectRatio)
{
    float xDepth = static_cast<float>(_extends.max.x - _extends.min.x);
    float yDepth = static_cast<float>(_extends.max.y - _extends.min.y);
    float zDepth = static_cast<float>(_extends.max.z - _extends.min.z);
    float averageDepth = (xDepth + yDepth + zDepth) / 3.f;
    m_zoomBase = static_cast<float>(gltf::utils::getFittingCameraZoom(_extends, _yFov, _aspectRatio));
    setZoom(m_zoomBase);
    constexpr float zoomSpeedDenominator = 2500.f;
    constexpr float panSpeedBaseDenominator = 1200.f;
    constexpr float freeCamSpeedDenominator = 2.f;
    m_zoomSpeed = averageDepth / zoomSpeedDenominator;
    m_freeCamSpeedBase = averageDepth / freeCamSpeedDenominator;
    m_freeCamSpeed = m_freeCamSpeedBase;
    m_panSpeedBase = averageDepth / panSpeedBaseDenominator;
    m_panSpeed = m_panSpeedBase;
}

void CameraActor::updateUserCameraTransform()
{
    if (m_pUserCamera == nullptr) {
        return;
    }
    const math::vec3 offset = toLocalRotation(math::vec3(0.f, 0.f, m_zoom));
    const math::vec3 position = m_lookTarget + offset;

    math::vec3 up = math::vec3(0, 1, 0);
    if (math::all(math::epsilonEqual(up, math::normalize(offset), math::vec3(1.0e-5f)))) {
        // in case we are looking at the object from the top our up vector is along +Z
        up = math::vec3(0, 0, -1);
    } else if (math::all(math::epsilonEqual(-up, math::normalize(offset), math::vec3(1.0e-5f)))) {
        // in case we are looking at the object from the bottom our up vector is along -Z
        up = math::vec3(0, 0, 1);
    }

    const math::mat4 view = math::lookAtRH(position, m_lookTarget, up);
    const math::mat4 transform = math::inverse(view);
    m_pUserCamera->setTransform(transform);
}

void CameraActor::rotateCamera(int32_t _deltaX, int32_t _deltaY)
{
    if (m_pUserCamera == nullptr) {
        return;
    }
    constexpr float rotationSpeed = 1.f / 180.f;
    constexpr float yRotationMax = math::pi<float>() / 2.f - 0.01f;
    m_rotationX += rotationSpeed * static_cast<float>(_deltaX);
    m_rotationY += rotationSpeed * static_cast<float>(_deltaY);
    m_rotationY = math::clamp(m_rotationY, -yRotationMax, yRotationMax);

    updateUserCameraTransform();
}

void CameraActor::setCameraRotation(float _x, float _y)
{
    if (m_pUserCamera == nullptr) {
        return;
    }
    m_rotationX = _x;
    m_rotationY = _y;

    updateUserCameraTransform();
}

void CameraActor::orientUserCamera(CameraOrientation orientation)
{
    switch (orientation) {
    case CameraOrientation::Top:
        setCameraRotation(math::pi<float>(), math::pi<float>() / 2.f);
        break;
    case CameraOrientation::Bottom:
        setCameraRotation(math::pi<float>(), -math::pi<float>() / 2.f);
        break;
    case CameraOrientation::Right:
        setCameraRotation(-math::pi<float>() / 2.f, 0.f);
        break;
    case CameraOrientation::Left:
        setCameraRotation(math::pi<float>() / 2.f, 0.f);
        break;
    case CameraOrientation::Front:
        setCameraRotation(0.f, 0.f);
        break;
    case CameraOrientation::Back:
        setCameraRotation(math::pi<float>(), 0.f);
        break;
    case CameraOrientation::Isometric:
        setCameraRotation(math::pi<float>() * 0.25f, math::radians(30.f));
        break;
    }
}

void CameraActor::panCamera(int32_t _deltaX, int32_t _deltaY)
{
    if (m_pUserCamera == nullptr) {
        return;
    }
    constexpr math::vec3 down = math::vec3(0.f, -1.f, 0.f);
    constexpr math::vec3 right = math::vec3(1.f, 0.f, 0.f);
    constexpr float userPanSpeedScale = 1.0f;

    m_lookTarget += m_panSpeed * static_cast<float>(_deltaX) * toLocalRotation(right) * userPanSpeedScale;
    m_lookTarget += m_panSpeed * static_cast<float>(_deltaY) * toLocalRotation(down) * userPanSpeedScale;

    updateUserCameraTransform();
}

void CameraActor::zoomCamera(float _delta)
{
    if (m_pUserCamera == nullptr) {
        return;
    }
    constexpr float userZoomSpeedScale = 1.0f;

    m_zoom -= m_zoomSpeed * _delta * (m_zoom / m_zoomBase) * userZoomSpeedScale;
    setZoom(math::max(m_zoom, zoomMin));
    m_panSpeed = m_panSpeedBase * (m_zoom / m_zoomBase);

    updateUserCameraTransform();
    if (m_pUserCamera->getType() == renderer::Camera::Type::Orthographic) {
        auto* camera = m_pUserCamera;
        float ratio = atan(camera->getYFoV() / 2.0f);
        camera->setOrthographic(camera->getAspectRatio() * m_zoom * ratio, m_zoom * ratio);
    }
}

math::vec3 CameraActor::toLocalRotation(const math::vec3& _vector) const
{
    constexpr math::mat4 identity = math::identity<math::mat4>();
    constexpr math::vec3 up = math::vec3(0.f, 1.f, 0.f);
    constexpr math::vec3 right = math::vec3(1.f, 0.f, 0.f);

    math::mat4 rotation = math::rotate(identity, m_rotationY, right);
    rotation = math::rotate(rotation, m_rotationX, up);

    return math::vec4(_vector, 1.f) * rotation;
}

math::vec3 CameraActor::getEyePosition() const
{
    const math::vec3 offset = toLocalRotation(math::vec3(0.f, 0.f, m_zoom));
    return m_lookTarget + offset;
}

void CameraActor::handleFreeCamera(int32_t _mouseDeltaX, int32_t _mouseDeltaY, const math::vec3& _velocity)
{
    if (m_pUserCamera == nullptr) {
        return;
    }
    constexpr float rotationSpeed = 0.005f;
    constexpr float freeCamSpeedScale = 1.0f;

    const float newYaw = _mouseDeltaX * rotationSpeed + m_lastCamYaw;
    m_lastCamYaw = newYaw;
    float newPitch = _mouseDeltaY * rotationSpeed + m_lastCamPitch;

    // clamp
    constexpr float freeCameraPitchBias = 0.1f;
    constexpr float maxPitch = math::pi<float>() / 2.f - freeCameraPitchBias;
    newPitch = math::clamp(newPitch, -maxPitch, maxPitch);
    m_lastCamPitch = newPitch;

    // calculate new position
    math::vec3 camPos = m_pUserCamera->getPositon();

    math::vec3 forward = math::normalize(m_pUserCamera->getView());

    math::vec3 up = math::vec3(0, 1, 0);
    if (math::all(math::epsilonEqual(up, forward, math::vec3(1.0e-5f)))) {
        // in case we are looking at the object from the top our up vector is along +Z
        up = math::vec3(0, 0, -1);
    } else if (math::all(math::epsilonEqual(-up, forward, math::vec3(1.0e-5f)))) {
        // in case we are looking at the object from the bottom our up vector is along -Z
        up = math::vec3(0, 0, 1);
    }

    math::vec3 right = math::cross(up, forward);

    math::vec3 velocity = forward * _velocity.x + right * _velocity.z + up * _velocity.y;

    camPos += velocity * m_freeCamSpeed * freeCamSpeedScale;

    const float cosPitch = cos(newPitch);
    const float sinPitch = sin(newPitch);
    const float cosYaw = cos(newYaw);
    const float sinYaw = sin(newYaw);
    math::vec3 xaxis(cosYaw, 0, -sinYaw);
    math::vec3 yaxis(sinYaw * sinPitch, cosPitch, cosYaw * sinPitch);
    math::vec3 zaxis(sinYaw * cosPitch, -sinPitch, cosPitch * cosYaw);

    // Create a 4x4 view matrix from the right, up, forward and eye position vectors
    math::mat4 viewMatrix(
        math::vec4(xaxis.x, yaxis.x, zaxis.x, 0),
        math::vec4(xaxis.y, yaxis.y, zaxis.y, 0),
        math::vec4(xaxis.z, yaxis.z, zaxis.z, 0),
        math::vec4(-dot(xaxis, camPos), -dot(yaxis, camPos), -dot(zaxis, camPos), 1));
    m_pUserCamera->setTransform(math::inverse(viewMatrix));

    // move look target
    math::vec3 newViewDir = m_pUserCamera->getView();
    newViewDir = math::normalize(newViewDir);
    m_lookTarget = camPos - newViewDir * m_zoom;
}

void CameraActor::applyOrbitCamOrientationToFreeCam()
{
    if (m_pUserCamera == nullptr) {
        return;
    }
    constexpr float pi = math::pi<float>();
    constexpr math::vec3 forward = math::vec3(0.f, 0.f, 1.f);
    constexpr math::vec3 up = math::vec3(0.f, 1.f, 0.f);

    math::vec3 viewDir = m_pUserCamera->getView();

    //compute angle
    math::vec3 viewDirXZPlane = math::vec3(viewDir.x, 0.f, viewDir.z);
    if (viewDirXZPlane == math::vec3(0)) {
        // in case the camera is aligned with the Y axis we must construct a "right" vector
        viewDirXZPlane = math::vec3(0, 0, 1);
    }
    viewDirXZPlane = math::normalize(viewDirXZPlane);

    float angleX = math::angle(viewDirXZPlane, forward);

    // fix angle
    if (viewDirXZPlane.x < 0.f) {
        angleX = (2.f * pi) - angleX;
    }
    m_lastCamYaw = angleX;

    // compute new x angle needed for correct y angle
    angleX = math::angle(viewDirXZPlane, forward);
    // fix angle
    angleX = (pi)-angleX;
    if (viewDirXZPlane.x < 0.f) {
        angleX = (pi * 2.f) - angleX;
    }

    // calculate correct y angle
    constexpr math::vec3 backward = math::vec3(0.f, 0.f, -1.f);
    constexpr math::mat4 identity = math::identity<math::mat4>();
    math::mat4 rotation = math::rotate(identity, angleX, up);
    math::vec3 vec = math::vec4(backward, 1.f) * rotation;
    float angleY = math::angle(math::normalize(vec), viewDir);
    if (viewDir.y > 0.f) {
        angleY = -angleY;
    }
    m_lastCamPitch = angleY;
}

// calculates correct orbit camera rotation from free camera rotation system.
void CameraActor::applyFreeCamOrientationToOrbitCam()
{
    if (m_pUserCamera == nullptr) {
        return;
    }
    constexpr float pi = math::pi<float>();
    constexpr math::vec3 forward = math::vec3(0.f, 0.f, 1.f);
    constexpr math::vec3 up = math::vec3(0.f, 1.f, 0.f);

    math::vec3 camPos = m_pUserCamera->getPositon();
    math::vec3 diff = math::normalize(m_lookTarget - camPos);

    //compute angle
    math::vec3 viewDirXZPlane = math::vec3(diff.x, 0.f, diff.z);
    if (viewDirXZPlane == math::vec3(0)) {
        // in case the camera is aligned with the Y axis we must construct a "right" vector
        viewDirXZPlane = math::vec3(0, 0, 1);
    }
    viewDirXZPlane = math::normalize(viewDirXZPlane);

    float angleX = math::angle(viewDirXZPlane, forward);
    // fix angle
    angleX = (pi)-angleX;
    if (viewDirXZPlane.x < 0.f) {
        angleX = (pi * 2.f) - angleX;
    }
    m_rotationX = angleX;

    constexpr math::mat4 identity = math::identity<math::mat4>();
    math::mat4 rotation = math::rotate(identity, angleX, up);
    math::vec3 vec = math::vec4(forward, 1.f) * rotation;
    vec = -vec;
    float angleY = math::angle(math::normalize(vec), diff);
    if (diff.y > 0.f) {
        angleY = -angleY;
    }

    m_rotationY = angleY;
}
