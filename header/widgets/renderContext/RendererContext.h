#pragma once

#include "QtGL.h"
#include "enums/EditorRenderLayerEnums.h"
#include "helpers/QtFileSystem.h"
#include "interfaces/GltfInteractorOutput.h"
#include "interfaces/LoadInteractorOutput.h"
#include "interfaces/RenderSettingInteractorOutput.h"
#include "model/EnvironmentMapBinding.h"
#include "renderViewInput/CameraActor.h"
#include "renderer/Renderer.h"
#include "renderer/editor/EditorRenderLayer.h"

#include <QColor>

#include <optional>

class QOpenGLFramebufferObject;

namespace ux3d::renderer::gl {
class Texture;
}

namespace ux3d::renderer {
class GltfRenderLayer;
class PickingRenderLayer;
class CommandGenerator;
class AudioRenderer;
}

namespace ux3d::filesystem {
class Path;
}

namespace ux3d::editor {
class CameraActor;

struct RenderParameters {
    RenderParameters();

    std::optional<float> animationTime;

    RenderMode renderMode = RenderMode::None;

    std::optional<uint32_t> materialVariantID = std::nullopt;

    gltf::Scene* pActiveGltfScene = nullptr;
    int64_t activeGltfSceneID = -1;
    int64_t activeGltfCameraNodeId = -1;
    gltf::Gltf* pActiveGltf = nullptr;
    int width = 1;
    int height = 1;

    uint32_t multiSampleCount;

    bool drawHelpers = false;

    bool iblEnabled = true;
    bool cubeMapEnabled = true;
    bool punctualLightsEnabled = true;
    QColor clearColor = QColor();
    renderer::ToneMappingsEnum toneMapping = renderer::ToneMappingsEnum::None;
    float exposure = 1.0f;
    float envMapRotation = 0;
    float envMapBlur = 0;
    float envMapIntensity = 1;
    filesystem::Path envMapPath = filesystem::Path(QtFileSystem::instance());

    bool isSkinningEnabled = true;
    bool isMorphingEnabled = true;
    bool isPbrSpecularGlossinessEnabled = true;
    bool isUnlitEnabled = true;
    bool isClearCoatEnabled = true;
    bool isSheenEnabled = true;
    bool isTransmissionEnabled = true;
    bool isVolumeEnabled = true;
    bool isIridescenceEnabled = true;
    bool isIOREnabled = true;
    bool isSpecularEnabled = true;
    bool isEmissiveStrengthEnabled = true;
};

struct RendererState {
    RendererState(renderer::gl::GL* gl)
        : renderer(gl)
    {
    }

    renderer::AudioRenderer* audioRenderer = nullptr;
    renderer::GltfRenderLayer* gltfLayer = nullptr;
    renderer::EditorRenderLayer* editorLayer = nullptr;
    renderer::PickingRenderLayer* pickingLayer = nullptr;
    renderer::CommandGenerator* commandGenerator = nullptr;
    renderer::Renderer renderer;

    QOpenGLFramebufferObject* framebuffer = nullptr;
    QOpenGLFramebufferObject* pickingFramebuffer = nullptr;
    QOpenGLFramebufferObject* selectionFramebuffer = nullptr;
    QOpenGLFramebufferObject* transmissionFramebuffer = nullptr;
};

// This class only handles rendering.
// All book keeping related tasks have to be performed by another class.
class RendererContext {
public:
    // rendererState and RendererParameters will be deleted in this objects destructor
    RendererContext();
    ~RendererContext();

    // sets correct values (env map, etc) to gltfRenderLayer
    // and command generator based on the settings in m_renderParam.
    void setGltfRenderLayerParamsAndComGenFromParameters();

    // call the initGL function to ensure all resources are recreated (e.g. when the context switches)
    // creates gltfLayer, editorLayer, pickingLayer and commandGenerator
    void initGL(bool createAudio = true);

    void setDimensions(uint32_t _width, uint32_t _height);
    void paintGL(float deltaTime);

    // callback function for project manager dirty state
    void dirtyStateChanged();

    const renderer::ReverseMapping& getReverseMapping() const { return m_reverseMapping; }
    const RendererState& getRenderState() const { return m_renderState; }

    RenderParameters renderParameters;

    RendererState m_renderState;

private:
    // deletes render layers and command generator in struct
    void cleanup();

    renderer::QtGL m_gl;

    renderer::ReverseMapping m_reverseMapping;
    bool m_gltfRenderLayerNeedsUpdate = false;
};
}
