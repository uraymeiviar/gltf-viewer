// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/Camera.h"
#include "math/MatrixFunctions.h"
#include "math/VectorFunctions.h"
#include "math/VectorTypes.h"

using namespace ux3d::renderer;

Camera::Camera(gl::GL* gl)
    : m_projectionMatrix(1.0f)
    , m_viewMatrix(1.0f)
    , m_position(0.f)
    , m_viewUniform("u_ViewMatrix")
    , m_projectionUniform("u_ProjectionMatrix")
    , m_viewProjectionUniform("u_ViewProjectionMatrix")
    , m_cameraPositionUniform("u_Camera")
    , m_cameraOrthographicScalingUniform("u_orthographicScale")
    , m_viewportResolutionUniform("u_viewportResolution")
{
    m_viewportResolutionUniform.write(m_viewportResolution);
}

Camera::Camera(const Camera& _Other)
    : m_type(_Other.m_type)
    , m_yFoV(_Other.m_yFoV)
    , m_aspectRatio(_Other.m_aspectRatio)
    , m_zNear(_Other.m_zNear)
    , m_zFar(_Other.m_zFar)
    , m_xMag(_Other.m_xMag)
    , m_yMag(_Other.m_yMag)
    , m_projectionMatrix(_Other.m_projectionMatrix)
    , m_viewMatrix(_Other.m_viewMatrix)
    , m_position(_Other.m_position)
    , m_viewUniform(_Other.m_viewUniform)
    , m_projectionUniform(_Other.m_projectionUniform)
    , m_viewProjectionUniform(_Other.m_viewProjectionUniform)
    , m_cameraPositionUniform(_Other.m_cameraPositionUniform)
    , m_cameraOrthographicScalingUniform(_Other.m_cameraOrthographicScalingUniform)
    , m_viewportResolutionUniform(_Other.m_viewportResolutionUniform)
{
}

Camera::Camera(Camera&& _Other)
    : m_type(_Other.m_type)
    , m_yFoV(_Other.m_yFoV)
    , m_aspectRatio(_Other.m_aspectRatio)
    , m_zNear(_Other.m_zNear)
    , m_zFar(_Other.m_zFar)
    , m_xMag(_Other.m_xMag)
    , m_yMag(_Other.m_yMag)
    , m_projectionMatrix(_Other.m_projectionMatrix)
    , m_viewMatrix(_Other.m_viewMatrix)
    , m_position(_Other.m_position)
    , m_viewUniform(std::move(_Other.m_viewUniform))
    , m_projectionUniform(std::move(_Other.m_projectionUniform))
    , m_viewProjectionUniform(std::move(_Other.m_viewProjectionUniform))
    , m_cameraPositionUniform(std::move(_Other.m_cameraPositionUniform))
    , m_cameraOrthographicScalingUniform(std::move(_Other.m_cameraOrthographicScalingUniform))
    , m_viewportResolutionUniform(std::move(_Other.m_viewportResolutionUniform))
{
}

void ux3d::renderer::Camera::updateTransform(const math::mat4x4& _transform)
{
    setTransform(_transform);
}

void Camera::setViewUniformName(const ux3d::core::string& _name)
{
    m_viewUniform.name = _name;
}

void Camera::setProjectionUniformName(const ux3d::core::string& _name)
{
    m_projectionUniform.name = _name;
}

void Camera::setViewProjectionUniformName(const ux3d::core::string& _name)
{
    m_viewProjectionUniform.name = _name;
}

void Camera::setCameraPositionUniformName(const ux3d::core::string& _name)
{
    m_cameraPositionUniform.name = _name;
}

void Camera::setTransform(const ux3d::math::mat4x4& _transform)
{
    m_position = _transform[3];
    m_cameraPositionUniform.write(m_position);
    m_viewMatrix = math::inverse(_transform);
    updateViewProjection();
}

void ux3d::renderer::Camera::setProjection(const math::mat4x4& _projection)
{
    m_projectionMatrix = _projection;
    updateViewProjection();
}

void Camera::setPerspective(float _aspectRatio, float _yFoV, float _zNear, float _zFar)
{
    m_type = Type::Perspective;

    m_aspectRatio = _aspectRatio;
    m_yFoV = _yFoV;
    m_zNear = _zNear;
    m_zFar = _zFar;

    if (m_zFar >= 0.0) {
        // finite projection
        m_projectionMatrix = math::perspectiveRH(m_yFoV, m_aspectRatio, m_zNear, m_zFar);
    } else {
        // infinite projectio
        m_projectionMatrix = math::infinitePerspectiveRH(m_yFoV, m_aspectRatio, m_zNear);
    }

    updateViewProjection();
}

void ux3d::renderer::Camera::setOrthographic(float _xMag, float _yMag, float _zNear, float _zFar)
{
    m_type = Type::Orthographic;

    m_xMag = _xMag;
    m_yMag = _yMag;

    m_zNear = _zNear;
    m_zFar = _zFar;

    // https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#orthographic-projection
    m_projectionMatrix = math::mat4x4(1.f);

    m_projectionMatrix[0][0] = 1.f / m_xMag;
    m_projectionMatrix[1][1] = 1.f / m_yMag;
    m_projectionMatrix[2][2] = 2.f / (m_zNear - m_zFar);
    m_projectionMatrix[3][2] = (m_zFar + m_zNear) / (m_zNear - m_zFar);

    // m_projectionMatrix = math::orthoRH(-m_xMag, m_xMag, m_yMag, -m_yMag, m_zNear, m_zFar);

    updateViewProjection();
}

void ux3d::renderer::Camera::setOrthographic(float _xMag, float _yMag)
{
    setOrthographic(_xMag, _yMag, m_zNear, m_zFar);
}

void ux3d::renderer::Camera::changeType(Type _type, float _Zoom)
{
    if (_type == Type::Orthographic) {
        float ratio = atan(m_yFoV / 2.0f);
        setOrthographic(_Zoom * m_aspectRatio * ratio, _Zoom * ratio);
    } else {
        setPerspective(m_aspectRatio, m_yFoV, m_zNear, m_zFar);
    }
}

void Camera::setAspectRatio(float aspectRatio)
{
    if (m_type == Type::Orthographic) {
        m_aspectRatio = aspectRatio; // set aspect ratio here cause we need it in getOrthographicScaling to get correct x scalling
        setOrthographic(m_yMag * aspectRatio, m_yMag, m_zNear, m_zFar);
    } else {
        setPerspective(aspectRatio, m_yFoV, m_zNear, m_zFar);
    }
}

void Camera::setViewportResolution(int width, int height)
{
    setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    m_viewportResolution = { width, height };
    m_viewportResolutionUniform.write(m_viewportResolution);
}

float Camera::getYFoV() const
{
    return m_yFoV;
}

void Camera::updateViewProjection()
{
    const math::mat4 viewProj = m_projectionMatrix * m_viewMatrix;
    m_viewUniform.write(m_viewMatrix);
    m_projectionUniform.write(m_projectionMatrix);
    m_viewProjectionUniform.write(viewProj);

    // also update the orthographic scaling uniform, as the scaling might have changed
    m_cameraOrthographicScalingUniform.write(getOrthographicScaling());
}

ux3d::math::vec3 Camera::getView() const
{
    constexpr auto identity = math::vec4(0, 0, 1, 0);
    return math::normalize(math::vec3(identity * getViewMatrix()));
}

ux3d::math::vec3 Camera::getUp() const
{
    // returns up direction of the camera
    constexpr auto identity = math::vec4(0, 1, 0, 0);
    return math::normalize(math::vec3(identity * getViewMatrix()));
}

float Camera::getDistance(ux3d::math::vec3 position) const
{
    const auto vector = getPositon() - position;
    // only the depth is relevant
    return math::dot(getView(), vector);
}

float Camera::getOrthographicScaling() const
{
    return m_type == Type::Orthographic ? 1.8f * (m_xMag / m_aspectRatio) : 1.0f;
}
