#pragma once

#include "enums/EditorRenderLayerEnums.h"
#include "helpers/QtFileSystem.h"
#include "interfaces/GltfInteractorOutput.h"
#include "interfaces/LoadInteractorOutput.h"
#include "interfaces/RenderSettingInteractorOutput.h"
#include "model/EnvironmentMapBinding.h"
#include "model/MaterialVariantsListBinding.h"
#include "renderViewInput/CameraActor.h"
#include "renderViewInput/RenderViewInput.h"
#include "renderer/Renderer.h"
#include "renderer/editor/EditorRenderLayer.h"
#include "renderer/enums/ToneMappingsEnum.h"

#include <QColor>
#include <QOpenGLWidget>
#include <chrono>
#include <memory>
#include <optional>
#include <queue>
#include <tuple>

using Clock = std::chrono::steady_clock;

namespace Qt {
enum ApplicationState;
}

namespace ux3d::renderer::gl {
class Texture;
}

namespace ux3d::renderer {
class GltfRenderLayer;
class PickingRenderLayer;
class CommandGenerator;
}

namespace ux3d::filesystem {
class Path;
}

namespace ux3d::editor {
class RendererContext;

struct CaptureConfiguration {
    uint32_t width = 0;
    uint32_t height = 0;
    int32_t gltfSceneIndex = -1;
    core::string outputFile = "";
    uint32_t renderQuality = 100;
    std::optional<uint32_t> gltfCameraIndex;
    std::optional<int32_t> numberOfFrames;
    int32_t framerate = 60;
    std::optional<int32_t> startFrameIndex;
    bool animationEnabled = false;
    bool fitUserCameraToScene = true;
    std::optional<renderer::ToneMappingsEnum> toneMapping;
    std::optional<QColor> clearColor;
    bool alphaChannel = false;
    std::optional<float> vFoV;
    std::optional<float> radius;
    std::optional<float> theta;
    std::optional<float> phi;
    std::optional<math::vec3> target;

    filesystem::Path environmentMapPath = filesystem::Path(QtFileSystem::instance());

    std::function<void(bool busy)> callbackSuccess = [](bool b) {};
    std::function<void(bool busy)> callbackError = [](bool b) {};
};

enum class CameraOrientation {
    Top = 0u,
    Bottom,
    Right,
    Left,
    Front,
    Back
};

class GLTFRendererWidget : public QOpenGLWidget,
                           public LoadInteractorOutput,
                           public GltfInteractorOutput,
                           public RenderSettingInteractorOutput,
                           public model::IEnvironmentMapPresentation,
                           public model::IMaterialVariantsListPresentation {
    Q_OBJECT

public:
    explicit GLTFRendererWidget(QWidget* parent = nullptr);
    ~GLTFRendererWidget() override;

    void presentGltfOpen(gltf::Gltf* pGltf) final;
    void presentGltfReloaded(gltf::Gltf* gltf) final;
    void presentSceneOpen(const GltfSceneOpenUseCase& useCase) final;
    void presentIBLEnabled(bool enabled) final;
    void presentPunctualLightsEnabled(bool enabled) final;
    void presentToneMappingChanged(renderer::ToneMappingsEnum toneMapping) final;
    void presentExposureChanged(float exposure) final;
    void presentSkinningEnabled(bool enable) final;
    void presentMorphingEnabled(bool enable) final;
    void presentPbrSpecularGlossinessEnabled(bool enabled) final;
    void presentUnlitEnabled(bool enabled) final;
    void presentClearCoatEnabled(bool enabled) final;
    void presentSheenEnabled(bool enabled) final;
    void presentTransmissionEnabled(bool enabled) final;
    void presentVolumeEnabled(bool enabled) final;
    void presentIridescenceEnabled(bool enabled) final;
    void presentIOREnabled(bool enabled) final;
    void presentSpecularEnabled(bool enabled) final;
    void presentEmissiveStrengthEnabled(bool enabled) final;
    void presentCubeMapEnabled(bool enabled) final;
    void presentEnvironmentMapPath(const filesystem::Path& path) final;
    void presentEnvironmentMapRotation(float rotation) final;
    void presentEnvironmentMapBlurNormalized(float blur) final;
    void presentEnvironmentMapIntensity(float value) final;
    void presentMaterialVariantsSelected(std::optional<uint32_t> _materialVariantID) final;

    void useGltfCameraByNode(const gltf::Node* pNode);
    void useUserCamera();
    void orientUserCamera(CameraOrientation orientation);
    void createGLTFCameraFromFreeCamera();
    renderer::GltfRenderLayer* getGltfRenderLayer() const;
    RenderViewInput& getRenderViewInput() { return m_input; }
    CameraActor& getCameraActor() { return m_cameraActor; }

    void setAnimationTime(const std::optional<float>& time);
    void setRenderMode(RenderMode renderMode);
    void setShowBoundingBoxes(bool show);
    void setShowSkeletons(bool show);

    void setGizmoState(renderer::GizmoState state) { m_gizmoState = state; }
    virtual void setGizmoVisibility(bool isVisible) = 0;
    virtual void setGizmoSpace(GizmoSpace gizmoSpace) = 0;
    virtual GizmoSpace getGizmoSpace() const = 0;

    virtual void performPickingInNextFrame() = 0;

    double getDevicePixelRatio() const;
    uint32_t getWidth() const;
    uint32_t getHeight() const;

    const gltf::Scene* getCurrentScene() const;

    void queueCapture(const CaptureConfiguration& configuration);

    void playAudio();
    void stopAudio();
    void rewindAudio();
    void pauseAudio();
    void toggleMute(bool unmute);
    void setAudioPlaybackTime(float t);

signals:
    void rendererReady();
    void integrateTime(float dt);

public slots:
    void updateEnvironmentMap(const model::EnvironmentMapBinding& environmentMapBinding);
    void iblMaxResolutionExpChanged();
    void renderCapture();

protected:
    void initializeGL() override;
    void paintGL() override;

    void performPicking();

    void capture(const CaptureConfiguration& configuration);

    void tabletEvent(QTabletEvent* event) final;

    std::unique_ptr<RendererContext> m_rendererContext;

    CameraActor m_cameraActor;
    RenderViewInput m_input;

    model::EnvironmentMapBinding m_environmentMap;

    renderer::GizmoState m_gizmoState = {};
    QImage m_pickingBuffer; // RGB
    bool m_isPicking = false;

    bool m_useViewportResolution = false;

private:
    std::queue<CaptureConfiguration> m_renderCaptureQueue;

    void createViewportResolutionPopup();
    std::optional<std::tuple<double, double>> m_viewportResolution;

    Clock::time_point m_frameTime;
    bool m_cameraNeedsUpdate = false;
};
}
