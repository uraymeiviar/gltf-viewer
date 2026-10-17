// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace ux3d::imgui {

enum class PresetView {
    Front,
    Back,
    Right,
    Left,
    Top,
    Bottom,
    Isometric
};

class OrbitCamera {
public:
    OrbitCamera();

    void setViewportSize(float width, float height);
    void setPerspective(float fovYDegrees, float zNear, float zFar);
    void setFoVY(float fovYDegrees) { m_fovY = std::clamp(fovYDegrees, 1.0f, 160.0f); }

    void rotate(float deltaYawDegrees, float deltaPitchDegrees);
    void pan(float deltaX, float deltaY);
    void zoom(float deltaZoom);
    void fitToBounds(const glm::vec3& minBounds, const glm::vec3& maxBounds);
    void reset();

    void setPresetView(PresetView view);
    bool isOrthographic() const { return m_isOrtho; }
    void setOrthographic(bool ortho) { m_isOrtho = ortho; }
    void toggleProjection() { m_isOrtho = !m_isOrtho; }

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec3 getEyePosition() const;

    const glm::vec3& getTarget() const { return m_target; }
    float getDistance() const { return m_distance; }
    float getYaw() const { return m_yaw; }
    float getPitch() const { return m_pitch; }
    float getFoVY() const { return m_fovY; }
    float getAspectRatio() const { return m_aspectRatio; }
    float getZNear() const { return m_zNear; }
    float getZFar() const { return m_zFar; }

private:
    glm::vec3 m_target = glm::vec3(0.0f, 0.0f, 0.0f);
    float m_distance = 3.0f;
    float m_yaw = 45.0f;
    float m_pitch = 25.0f;

    float m_fovY = 45.0f;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_zNear = 0.01f;
    float m_zFar = 1000.0f;
    bool m_isOrtho = false;
};

} // namespace ux3d::imgui
