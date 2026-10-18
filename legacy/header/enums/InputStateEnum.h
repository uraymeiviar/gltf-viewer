#pragma once

#include <cstdint>

namespace ux3d::editor {
enum class RenderViewState : uint32_t {
    NotInitialized = 0u,
    PerspectiveCam,
    OrthographicCam,
    OrbitCam,
    RenderViewCam,
    FixedCam,
    FixedCamEdit
};

enum class InputToolState : uint32_t {
    Selection = 0u,
    Translation,
    Rotation,
    Scaling
};

enum class InputRenderViewState : uint32_t {
    None = 0u,
    Panning,
    Orbit,
    Zoom,
    Flight,
    //begin RenderViewInput::isSetPosAllowed false
    TranslateX,
    TranslateY,
    TranslateZ,
    TranslateXY,
    TranslateYZ,
    TranslateXZ,
    RotateX,
    RotateY,
    RotateZ,
    ScaleX,
    ScaleY,
    ScaleZ,
    ScaleXY,
    ScaleYZ,
    ScaleXZ,
    ScaleXYZ
    //end RenderViewInput::isSetPosAllowed false
};
}
