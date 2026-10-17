// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/OrbitCamera.h"

namespace ux3d::imgui {

OrbitCamera::OrbitCamera()
{
    reset();
}

void OrbitCamera::reset()
{
    m_target = glm::vec3(0.0f, 0.0f, 0.0f);
    m_distance = 3.0f;
    m_yaw = 45.0f;
    m_pitch = 25.0f;
    m_zNear = 0.01f;
    m_zFar = 1000.0f;
}

void OrbitCamera::setViewportSize(float width, float height)
{
    if (height > 0.0f && width > 0.0f) {
        m_aspectRatio = width / height;
    }
}

void OrbitCamera::setPerspective(float fovYDegrees, float zNear, float zFar)
{
    m_fovY = fovYDegrees;
    m_zNear = zNear;
    m_zFar = zFar;
}

void OrbitCamera::rotate(float deltaYawDegrees, float deltaPitchDegrees)
{
    m_yaw += deltaYawDegrees;
    m_pitch += deltaPitchDegrees;

    // Clamp pitch to avoid gimbal flip
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);

    // Keep yaw normalized in [0, 360)
    while (m_yaw < 0.0f) m_yaw += 360.0f;
    while (m_yaw >= 360.0f) m_yaw -= 360.0f;
}

void OrbitCamera::pan(float deltaX, float deltaY)
{
    float pitchRad = glm::radians(m_pitch);
    float yawRad = glm::radians(m_yaw);

    glm::vec3 forward(
        std::cos(pitchRad) * std::sin(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::cos(yawRad)
    );
    forward = glm::normalize(forward);

    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    float panSpeed = m_distance * 0.002f;
    m_target -= right * (deltaX * panSpeed);
    m_target += up * (deltaY * panSpeed);
}

void OrbitCamera::zoom(float deltaZoom)
{
    float zoomFactor = 1.0f - deltaZoom * 0.1f;
    m_distance *= zoomFactor;
    m_distance = std::clamp(m_distance, 0.01f, 10000.0f);
}

void OrbitCamera::fitToBounds(const glm::vec3& minBounds, const glm::vec3& maxBounds)
{
    m_target = (minBounds + maxBounds) * 0.5f;
    glm::vec3 size = maxBounds - minBounds;
    float maxDim = std::max({size.x, size.y, size.z});
    if (maxDim < 0.001f) maxDim = 1.0f;

    float fovRad = glm::radians(m_fovY);
    m_distance = (maxDim * 0.5f) / std::sin(fovRad * 0.5f) * 1.5f;
    m_zNear = std::max(0.001f, m_distance * 0.005f);
    m_zFar = std::max(1000.0f, m_distance * 20.0f);
}

glm::vec3 OrbitCamera::getEyePosition() const
{
    float pitchRad = glm::radians(m_pitch);
    float yawRad = glm::radians(m_yaw);

    glm::vec3 offset(
        m_distance * std::cos(pitchRad) * std::sin(yawRad),
        m_distance * std::sin(pitchRad),
        m_distance * std::cos(pitchRad) * std::cos(yawRad)
    );

    return m_target + offset;
}

glm::mat4 OrbitCamera::getViewMatrix() const
{
    return glm::lookAt(getEyePosition(), m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 OrbitCamera::getProjectionMatrix() const
{
    return glm::perspective(glm::radians(m_fovY), m_aspectRatio, m_zNear, m_zFar);
}

} // namespace ux3d::imgui
