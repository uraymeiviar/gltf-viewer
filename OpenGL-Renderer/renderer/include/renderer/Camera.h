// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "gl/GL.h"
#include "gl/Uniform.h"
#include "math/MatrixTypes.h"

#include "renderer/gltf/GltfCameraMapping.h"

namespace ux3d {
namespace renderer {
    class Camera {
        friend GltfCameraMapping;

    public:
        enum class Type {
            Custom = 0,
            Perspective,
            Orthographic
        };

        static constexpr float zfarInfinity = -1.0f;

        Camera(gl::GL* gl);
        Camera(const Camera&);
        Camera(Camera&&);

        void updateTransform(const math::mat4x4& transform);

        void setViewUniformName(const core::string& name);
        void setProjectionUniformName(const core::string& name);
        void setViewProjectionUniformName(const core::string& name);
        void setCameraPositionUniformName(const core::string& name);
        void setTransform(const math::mat4x4& transform); // view matrix
        void setProjection(const math::mat4x4& projection); // any projection

        // perspective projection
        void setPerspective(float aspectRatio, float yfov, float znear, float zfar);

        // orthographic projection
        void setOrthographic(float xMag, float yMag, float zNear, float zFar);
        void setOrthographic(float xMag, float yMag);

        void changeType(Type _type, float _Zoom);

        float getYFoV() const;

        void setViewportResolution(int width, int height);

        void setAspectRatio(float aspectRatio);
        float getAspectRatio() const
        {
            if (m_type == Type::Perspective) {
                return m_aspectRatio;
            } else {
                return m_xMag / m_yMag;
            }
        }

        const math::mat4x4& getViewMatrix() const { return m_viewMatrix; }
        const math::mat4x4& getProjectionMatrix() const { return m_projectionMatrix; }

        const gl::Uniform& getViewUniform() const { return m_viewUniform; }
        const gl::Uniform& getProjectionUniform() const { return m_projectionUniform; }
        const gl::Uniform& getViewProjectionUniform() const { return m_viewProjectionUniform; }
        const gl::Uniform& getCameraPositionUniform() const { return m_cameraPositionUniform; }
        const gl::Uniform& getOrthographicScalingUniform() const { return m_cameraOrthographicScalingUniform; }
        const gl::Uniform& getViewportResolutionUniform() const { return m_viewportResolutionUniform; }

        Type getType() const { return m_type; }
        const math::vec3& getPositon() const { return m_position; }

        math::vec3 getView() const;
        math::vec3 getUp() const;
        float getDistance(math::vec3 position) const;
        float getOrthographicScaling() const;

        float getZNearPlane() const { return m_zNear; }
        float getZFarPlane() const { return m_zFar; }

        // Orthographic half-extents, kept so the clipping planes can be edited in place.
        float getXMag() const { return m_xMag; }
        float getYMag() const { return m_yMag; }

        float getZoom() const { return m_zoom; }
        void setZoom(float _zoom) { m_zoom = _zoom; }

    private:
        void updateViewProjection();

        Type m_type = Type::Custom;

        float m_yFoV = 1.f;
        float m_aspectRatio = 1.f;
        float m_zNear = 0.1f;
        float m_zFar = 100.f;
        float m_xMag = 1.f;
        float m_yMag = 1.f;
        float m_zoom = 1.f;

        math::ivec2 m_viewportResolution = {};

        math::mat4x4 m_projectionMatrix;
        math::mat4x4 m_viewMatrix;
        math::vec3 m_position;

        gl::Uniform m_viewUniform;
        gl::Uniform m_projectionUniform;
        gl::Uniform m_viewProjectionUniform;
        gl::Uniform m_cameraPositionUniform;
        gl::Uniform m_cameraOrthographicScalingUniform;
        gl::Uniform m_viewportResolutionUniform;
    };
}
}
