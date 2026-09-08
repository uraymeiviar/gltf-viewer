// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "CameraOrthographic.h"
#include "CameraPerspective.h"
#include "ChildOfRoot.h"
#include <optional>

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#camera

namespace ux3d::gltf {
class Camera : public ChildOfRoot {
public:
    enum class ProjectionType : uint32_t {
        Perspective,
        Orthographic
    };

private:
    ProjectionType m_Type = ProjectionType::Perspective;
    std::optional<CameraPerspective> m_Perspective;
    std::optional<CameraOrthographic> m_Orthographic;

public:
    Camera(uint32_t _id, ProjectionType _projectionType); // regular constructor
    Camera(uint32_t _id, const Camera& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::Camera; }

    ProjectionType getType() const { return m_Type; }
    GltfResult setType(const ProjectionType& type);

    std::optional<CameraPerspective>& getCameraPerspective() { return m_Perspective; }
    const std::optional<CameraPerspective>& getCameraPerspective() const { return m_Perspective; }

    std::optional<CameraOrthographic>& getCameraOrthographic() { return m_Orthographic; }
    const std::optional<CameraOrthographic>& getCameraOrthographic() const { return m_Orthographic; }

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const override;
};
}
