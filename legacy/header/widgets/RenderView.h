#pragma once

#include "CustomDockWidget.h"
#include "interfaces/GltfInteractorOutput.h"
#include "interfaces/LoadInteractorOutput.h"
#include "interfaces/RenderSettingInteractorOutput.h"
#include "model/GltfBinding.h"
#include "ui_RenderView.h"

class QMenu;

namespace ux3d::editor {
class GLTFEditorWidget;
class AnimationPopup;

class RenderView : public CustomDockWidget,
                   public GltfInteractorOutput,
                   public LoadInteractorOutput,
                   public RenderSettingInteractorOutput,
                   private Ui::dockWidget_renderView {

    struct CameraInfo {
        RenderViewState cameraType = RenderViewState::NotInitialized;
        GltfPropertyPair nodeCameraPair;
    };

    Q_OBJECT
public:
    explicit RenderView(QWidget* pParent = nullptr);
    ~RenderView() override;

    GLTFEditorWidget* getRenderer() const;

    void gatherCameraMenu();

    void presentGltfReloaded(gltf::Gltf* gltf) final;
    void presentGltfOpen(gltf::Gltf* gltf) final;
    void presentGltfImport(gltf::Gltf* gltf) final;
    void presentSceneOpen(const GltfSceneOpenUseCase& useCase) final;
    void presentSkinningEnabled(bool enabled) final;
    void presentMorphingEnabled(bool enabled) final;

    bool getIsSkinningEnabled() { return m_isSkinningEnabled; }
    bool getIsMorphingEnabled() { return m_isMorphingEnabled; }

signals:
    void setRenderStateWireframe(bool isWireframe);

public slots:
    void cameraChanged(int index);
    void projectionChanged(RenderViewState cameraState = RenderViewState::NotInitialized);
    void renderModeChanged(RenderMode renderMode, bool emitWireFrameSignal = true);
    void updateCameraMenuEntry(const gltf::Node* pNode, const gltf::Camera* pCamera);

    // Animation
    void playAnimation();
    void rewindAnimation();
    void forwardAnimation();
    void stopAnimation();
    void sliderMoved(int value);
    void integrateTime(float deltaTime);

    void updateRenderStateWireframe(bool isWireframe);

private:
    CameraInfo m_renderModeCamera;
    CameraInfo m_editModeCamera;
    AnimationPopup* m_animationPopup = nullptr;

    void fillViewportModeMenu();
    void fillTools();
    void fillTimeline();
    void setViewportMode(bool isEditMode);
    static QString makeCameraMenuItemLabel(const gltf::Node* pNode, const gltf::Camera* pCamera);
    void fillRenderModeMenu();
    void showEvent(QShowEvent* event) override;
    void updateAnimationBarVisibility(const bool animationsPresent);

    QAction* m_pLastRenderModeAction = nullptr;
    void setRenderModeButtonText(QAction* action);

    model::GltfBinding m_gltfBinding;

    // Animation slider
    bool m_animationPlaying = false;
    float m_currentAnimationTime = 0.0f;
    float m_totalAnimationTime = 0.0f;
    constexpr static float sliderTicksPerSecond = 60.0f;
    void updateGltfAnimationTime();

    // Animation popup
    bool m_isSkinningEnabled = true;
    bool m_isMorphingEnabled = true;

    int64_t m_currentSceneId = -1;

    QIcon prevProjectionIcon;
    QString prevProjectionTooltip;
    QString prevProjectionText;
};
}
