#include "widgets/RenderView.h"
#include "Actions.h"
#include "Constants.h"
#include "PathConstants.h"
#include "enums/InputStateEnum.h"
#include "helpers/Name.h"
#include "interactors/GltfInteractor.h"
#include "interactors/GltfOperations.h"
#include "interactors/LoadInteractor.h"
#include "interactors/RenderSettingInteractor.h"
#include "model/ProjectManager.h"
#include "renderViewInput/CameraActor.h"
#include "useCases/GltfUseCases.h"
#include "widgets/MainWindow.h"
#include "widgets/popups/AnimationPopup.h"

#include <QActionGroup>
#include <QMenu>

#include <cmath>

using namespace ux3d;
using namespace ux3d::editor;

RenderView::RenderView(QWidget* parent)
    : CustomDockWidget(parent)
{
    // Note: Even though, this is a CustomDockWidget, we do not want this widget to be moved.
    //		 Qt and OpenGL do have an issue on this.
    setFeatures(QDockWidget::NoDockWidgetFeatures);
    Ui::dockWidget_renderView::setupUi(this);
    setObjectName("renderview");

    connect(Actions::instance().projectionPerspective, &QAction::triggered, [this] { projectionChanged(RenderViewState::PerspectiveCam); });
    connect(Actions::instance().projectionOrthographic, &QAction::triggered, [this] { projectionChanged(RenderViewState::OrthographicCam); });

    connect(pushButton_takeRender, &QPushButton::clicked, [this] { Actions::instance().renderCapture->trigger(); });
    connect(toolButton_projection, &QPushButton::clicked, [this] { projectionChanged(); });
    connect(comboBox_cameras, qOverload<int>(&QComboBox::currentIndexChanged), this, &RenderView::cameraChanged);

    pushButton_takeRender->setIcon(QIcon(pathConstants::iconRender));
    // by default we are in edit mode (when gestaltor starts) -> button should be disabled
    pushButton_takeRender->setEnabled(false);

    QSizePolicy sizePolicy = toolButton_grid->sizePolicy();
    sizePolicy.setRetainSizeWhenHidden(true);
    toolButton_grid->setSizePolicy(sizePolicy);
    toolButton_grid->setIcon(QIcon(pathConstants::iconGridOn));
    toolButton_grid->setCheckable(true);
    toolButton_grid->setChecked(true);
    toolButton_grid->setToolTip("Toggle to Hide Grid");

    connect(toolButton_grid, &QToolButton::toggled, [this](bool checked) {
        openGLWidget->showGrid(checked);
        if (checked) {
            toolButton_grid->setIcon(QIcon(pathConstants::iconGridOn));
            toolButton_grid->setToolTip("Toggle to Hide Grid");
        } else {
            toolButton_grid->setIcon(QIcon(pathConstants::iconGridOff));
            toolButton_grid->setToolTip("Toggle to Show Grid");
        }
    });

    sizePolicy = toolButton_projection->sizePolicy();
    sizePolicy.setRetainSizeWhenHidden(true);
    toolButton_projection->setSizePolicy(sizePolicy);
    toolButton_projection->setIcon(QIcon(pathConstants::iconPerspective));
    toolButton_projection->setToolTip("Toggle to Switch to Orthographic Projection");

    comboBox_cameras->hide();

    GltfInteractor::getInstance()->registerOutput(this);
    LoadInteractor::getInstance()->registerOutput(this);
    RenderSettingInteractor::getInstance()->registerOutput(this);

    // create dropdown menus (rendermode, debugmodes, etc)
    fillViewportModeMenu();
    fillRenderModeMenu();
    fillTools();

    connect(Actions::instance().showGrid, &QAction::toggled, toolButton_grid, &QToolButton::setChecked);
    connect(Actions::instance().showBoundingBoxes, &QAction::triggered, openGLWidget, &GLTFEditorWidget::setShowBoundingBoxes);
    connect(Actions::instance().showSkinsAndBones, &QAction::triggered, openGLWidget, &GLTFEditorWidget::setShowSkeletons);

    fillTimeline();

    horizontalLayout_top_general_layout->insertStretch(3);
}

RenderView::~RenderView()
{
    GltfInteractor::getInstance()->unregisterOutput(this);
    LoadInteractor::getInstance()->unregisterOutput(this);
    RenderSettingInteractor::getInstance()->unregisterOutput(this);
}

void RenderView::fillRenderModeMenu()
{
    QMenu* menu = new QMenu(pushButton_RenderMode);
    menu->addAction(Actions::instance().visualizationModeDefaultShaded);
    connect(Actions::instance().visualizationModeDefaultShaded, &QAction::triggered, [this] { renderModeChanged(RenderMode::None); });

    menu->addAction(Actions::instance().visualizationModeWireframe);
    connect(Actions::instance().visualizationModeWireframe, &QAction::triggered, [this] { renderModeChanged(RenderMode::Wireframe); });

    menu->addSeparator();
    menu->addActions(Actions::instance().visualizationModeDebugShadings);

    pushButton_RenderMode->setMenu(menu);
    connect(menu, &QMenu::triggered, this, [this](QAction* action) {
        setRenderModeButtonText(action);
        action->text();
        renderModeChanged(static_cast<RenderMode>(action->data().toInt()));
        m_pLastRenderModeAction = action;
    });
}

void RenderView::fillTools()
{
    if (auto* mainWindow = MainWindow::getMainWindow(); mainWindow != nullptr) {
        horizontalLayout_2->invalidate();
        horizontalLayout_2->activate();
        toolButton_select->setDefaultAction(Actions::instance().toolSelect);
        toolButton_move->setDefaultAction(Actions::instance().toolMove);
        toolButton_rotate->setDefaultAction(Actions::instance().toolRotate);
        toolButton_scale->setDefaultAction(Actions::instance().toolScale);

        float buttonHeight = toolButton_select->size().height();
        const QSize iconSize(buttonHeight * constants::toolButtonScaling, buttonHeight * constants::toolButtonScaling);
        toolButton_select->setIconSize(iconSize);
        toolButton_move->setIconSize(iconSize);
        toolButton_rotate->setIconSize(iconSize);
        toolButton_scale->setIconSize(iconSize);

        connect(Actions::instance().toolSelect, &QAction::triggered, [this](bool checked) {
            if (checked) {
                openGLWidget->getRenderViewInput().setInputToolState(InputToolState::Selection);
            }
        });
        connect(Actions::instance().toolMove, &QAction::triggered, [this](bool checked) {
            if (checked) {
                openGLWidget->getRenderViewInput().setInputToolState(InputToolState::Translation);
            }
        });
        connect(Actions::instance().toolRotate, &QAction::triggered, [this](bool checked) {
            if (checked) {
                openGLWidget->getRenderViewInput().setInputToolState(InputToolState::Rotation);
            }
        });
        connect(Actions::instance().toolScale, &QAction::triggered, [this](bool checked) {
            if (checked) {
                openGLWidget->getRenderViewInput().setInputToolState(InputToolState::Scaling);
            }
        });

        connect(Actions::instance().editSpaceWorld, &QAction::triggered, [this](bool) {
            openGLWidget->setGizmoSpace(GizmoSpace::Global);
            toolButton_gizmoSpace->setIcon(QIcon(pathConstants::iconWorldSpace));
            toolButton_gizmoSpace->setToolTip("Toggle to Local Space");
        });
        connect(Actions::instance().editSpaceLocal, &QAction::triggered, [this](bool) {
            openGLWidget->setGizmoSpace(GizmoSpace::Local);
            toolButton_gizmoSpace->setToolTip("Toggle to Global Space");
            toolButton_gizmoSpace->setIcon(QIcon(pathConstants::iconLocalSpace));
        });
        connect(Actions::instance().editSpaceToggle, &QAction::triggered, [this](bool checked) {
            auto space = openGLWidget->getGizmoSpace();
            if (space == GizmoSpace::Global) {
                Actions::instance().editSpaceLocal->trigger();
            } else if (space == GizmoSpace::Local) {
                Actions::instance().editSpaceWorld->trigger();
            }
        });
        toolButton_gizmoSpace->setDefaultAction(Actions::instance().editSpaceToggle);
        toolButton_gizmoSpace->setIcon(QIcon(pathConstants::iconWorldSpace));
        toolButton_gizmoSpace->setToolTip("Toggle to Local Space");
    }
}

void RenderView::fillTimeline()
{
    toolButton_selectAnimation->setDefaultAction(Actions::instance().selectAnimation);
    toolButton_resetBack->setDefaultAction(Actions::instance().rewindAnimation);
    toolButton_resetForward->setDefaultAction(Actions::instance().forwardAnimation);
    toolButton_playPause->setUseColorOverlay(true);
    toolButton_playPause->setDisabledColor(QColor(157, 157, 157));
    toolButton_playPause->setBaseIcon(QIcon(pathConstants::iconTimelinePlay));
    toolButton_stop->setDefaultAction(Actions::instance().stopAnimation);

    float buttonHeight = toolButton_select->size().height();
    QSize iconSize(buttonHeight * constants::timelineButtonScaling, buttonHeight * constants::timelineButtonScaling);
    toolButton_selectAnimation->setPressedColor(QColor(constants::animationToolButtonPressedIconColor));
    toolButton_resetBack->setIconSize(iconSize);
    toolButton_resetForward->setIconSize(iconSize);
    toolButton_playPause->setIconSize(iconSize);
    toolButton_stop->setIconSize(iconSize);

    connect(toolButton_selectAnimation, &QToolButton::clicked, [this] {
        if (m_animationPopup == nullptr || m_animationPopup->isHidden()) {
            QPoint position = toolButton_selectAnimation->mapToGlobal(
                QPoint(0, toolButton_selectAnimation->height()));
            m_animationPopup = new AnimationPopup(this, position);
            connect(m_animationPopup, &QWidget::destroyed, [this] {
                m_animationPopup = nullptr;
            });
            m_animationPopup->show();
        } else {
            m_animationPopup->close();
        }
    });

    connect(openGLWidget, &GLTFEditorWidget::integrateTime, this, &RenderView::integrateTime);
    connect(Actions::instance().rewindAnimation, &QAction::triggered, [this] {
        rewindAnimation();
        toolButton_playPause->setBaseIcon(QIcon(pathConstants::iconTimelinePlay));
    });
    connect(Actions::instance().forwardAnimation, &QAction::triggered, [this] {
        forwardAnimation();
        toolButton_playPause->setBaseIcon(QIcon(pathConstants::iconTimelinePlay));
    });
    connect(Actions::instance().stopAnimation, &QAction::triggered, [this] {
        stopAnimation();
        toolButton_playPause->setBaseIcon(QIcon(pathConstants::iconTimelinePlay));
    });
    connect(horizontalSlider_animationTime, &QAbstractSlider::sliderMoved, this, &RenderView::sliderMoved);
    connect(Actions::instance().playingAnimation, &QAction::triggered, this, &RenderView::playAnimation);
    connect(toolButton_playPause, &QAbstractButton::clicked, this, [this] {
        Actions::instance().playingAnimation->trigger();
    });
    connect(Actions::instance().playingAnimation, &QAction::changed, [this] {
        const bool animationsEnabled = Actions::instance().playingAnimation->isEnabled();
        horizontalSlider_animationTime->setEnabled(animationsEnabled);
        toolButton_playPause->setBaseIcon(QIcon(pathConstants::iconTimelinePlay));
        toolButton_playPause->setEnabled(animationsEnabled);
        updateAnimationBarVisibility(animationsEnabled);
    });
}

GLTFEditorWidget* RenderView::getRenderer() const
{
    return openGLWidget;
}

void RenderView::gatherCameraMenu()
{
    comboBox_cameras->clear();
    if (m_currentSceneId < 0) {
        comboBox_cameras->hide();
        return;
    }
    gltf::Gltf& currentGltf = model::ProjectManager::getInstance()->getGltf();
    gltf::Scene* currentScene = currentGltf.getByID<gltf::Scene>(static_cast<uint32_t>(m_currentSceneId));
    if (currentScene == nullptr) {
        comboBox_cameras->hide();
        m_currentSceneId = -1;
        return;
    }

    comboBox_cameras->blockSignals(true);
    comboBox_cameras->addItem("RenderView Camera");
    comboBox_cameras->blockSignals(false);

    // Gather per node, not by camera list since cameras can be used multiple times
    core::Vector<ux3d::gltf::Node*> allNodes;
    currentScene->getNodesRecursively(allNodes);
    for (const auto& pNode : allNodes) {
        const gltf::Camera* pCamera = pNode->getCamera();
        if (pCamera != nullptr) {
            updateCameraMenuEntry(pNode, pCamera);
        }
    }

    comboBox_cameras->show();
}

void RenderView::cameraChanged(int index)
{
    bool editMode = openGLWidget->isEditMode();
    if (index == 0 || index == -1) {
        auto rendererCameraType = renderer::Camera::Type::Perspective;
        auto uiCameraType = RenderViewState::RenderViewCam;
        openGLWidget->useUserCamera();
        m_renderModeCamera.cameraType = uiCameraType;
        openGLWidget->getCameraActor().changeUserCameraType(rendererCameraType);
        model::ProjectManager::getInstance()->unsetRenderViewCameraNodeID();
    } else {
        // glTF camera was selected
        GltfPropertyPair pair = comboBox_cameras->getPairAtIndex(index);
        const auto* pNode = pair.first.get<const gltf::Node*>();

        if (pNode != nullptr) {
            openGLWidget->useGltfCameraByNode(pNode);
            model::ProjectManager::getInstance()->setRenderViewCameraNodeID(pNode->m_ID);

            if (editMode) {
                m_editModeCamera.cameraType = RenderViewState::FixedCamEdit;
                m_editModeCamera.nodeCameraPair = pair;
                openGLWidget->getRenderViewInput().setRenderViewState(RenderViewState::FixedCamEdit);
            } else {
                m_renderModeCamera.cameraType = RenderViewState::FixedCam;
                m_renderModeCamera.nodeCameraPair = pair;
                openGLWidget->getRenderViewInput().setRenderViewState(RenderViewState::FixedCam);
            }
        }
    }

    Actions::instance().createCameraFromView->setEnabled(index == 0);
}

void RenderView::projectionChanged(RenderViewState state)
{
    renderer::Camera::Type rendererCameraType;
    RenderViewState uiCameraType;
    switch (state) {
    case RenderViewState::PerspectiveCam:
        rendererCameraType = renderer::Camera::Type::Perspective;
        uiCameraType = RenderViewState::PerspectiveCam;
        toolButton_projection->setText("Toggle to Switch to Orthographic Projection");
        toolButton_projection->setToolTip("Toggle to Switch to Orthographic Projection");
        toolButton_projection->setIcon(QIcon(pathConstants::iconPerspective));
        Actions::instance().projectionOrthographic->setChecked(false);
        Actions::instance().projectionPerspective->setChecked(true);
        break;
    case RenderViewState::OrthographicCam:
        rendererCameraType = renderer::Camera::Type::Orthographic;
        uiCameraType = RenderViewState::OrthographicCam;
        toolButton_projection->setText("Toggle to Switch to Perspective Projection");
        toolButton_projection->setToolTip("Toggle to Switch to Perspective Projection");
        toolButton_projection->setIcon(QIcon(pathConstants::iconOrtho));
        Actions::instance().projectionOrthographic->setChecked(true);
        Actions::instance().projectionPerspective->setChecked(false);
        break;
    default:
        if (toolButton_projection->text() == "Toggle to Switch to Orthographic Projection") {
            rendererCameraType = renderer::Camera::Type::Orthographic;
            uiCameraType = RenderViewState::OrthographicCam;
            toolButton_projection->setText("Toggle to Switch to Perspective Projection");
            toolButton_projection->setToolTip("Toggle to Switch to Perspective Projection");
            toolButton_projection->setIcon(QIcon(pathConstants::iconOrtho));
            Actions::instance().projectionOrthographic->setChecked(true);
            Actions::instance().projectionPerspective->setChecked(false);
        } else {
            rendererCameraType = renderer::Camera::Type::Perspective;
            uiCameraType = RenderViewState::PerspectiveCam;
            toolButton_projection->setText("Toggle to Switch to Orthographic Projection");
            toolButton_projection->setToolTip("Toggle to Switch to Orthographic Projection");
            toolButton_projection->setIcon(QIcon(pathConstants::iconPerspective));
            Actions::instance().projectionOrthographic->setChecked(false);
            Actions::instance().projectionPerspective->setChecked(true);
        }
        break;
    }
    m_editModeCamera.cameraType = uiCameraType;

    openGLWidget->getCameraActor().changeUserCameraType(rendererCameraType);
    openGLWidget->useUserCamera();
    openGLWidget->getRenderViewInput().setRenderViewState(uiCameraType);

    // store ui state
    prevProjectionIcon = toolButton_projection->icon();
    prevProjectionTooltip = toolButton_projection->toolTip();
    prevProjectionText = toolButton_projection->text();
}

void RenderView::renderModeChanged(RenderMode renderMode, bool emitWireFrameSignal)
{
    openGLWidget->setRenderMode(renderMode);

    if (emitWireFrameSignal == false) {
        return;
    }
    if (renderMode == RenderMode::Wireframe) {
        emit RenderView::setRenderStateWireframe(true);
    } else {
        emit RenderView::setRenderStateWireframe(false);
    }
}

void RenderView::presentGltfReloaded(gltf::Gltf* gltf)
{
    // check if any node/camera combination was previously used in RenderView
    std::optional<uint32_t> cameraNodeID = model::ProjectManager::getInstance()->getRenderViewCameraNodeID();
    if (cameraNodeID.has_value()) {
        for (const auto& node : gltf->getNodes()) {
            if (node.m_ID == cameraNodeID.value() && node.getCamera() != nullptr) {
                m_renderModeCamera.nodeCameraPair = GltfPropertyPair(&node, node.getCamera());
                break;
            }
        }
    }
    if (!openGLWidget->isEditMode()) {
        gatherCameraMenu();
    }

    const bool animationControlsEnabled = gltf != nullptr && (model::gltf_operations::hasAnimations() || model::gltf_operations::hasEmitters());

    Actions::instance().setAnimationControlsEnabled(animationControlsEnabled);
    Actions::instance().updateAudioMuteState();
    Actions::instance().updateAnimationPopupState();

    m_isSkinningEnabled = true;
    m_isMorphingEnabled = true;

    updateAnimationBarVisibility(animationControlsEnabled);
}

void RenderView::presentGltfOpen(gltf::Gltf* gltf)
{
    const bool animationControlsEnabled = gltf != nullptr && (model::gltf_operations::hasAnimations() || model::gltf_operations::hasEmitters());

    Actions::instance().setAnimationControlsEnabled(animationControlsEnabled);
    Actions::instance().updateAudioMuteState();
    Actions::instance().updateAnimationPopupState();
    setRenderModeButtonText(Actions::instance().visualizationModeDefaultShaded);
    Actions::instance().visualizationModeDefaultShaded->setChecked(true);
    m_gltfBinding.selectAllPossibleAnimations();
    model::ProjectManager::getInstance()->animationVisualizationChanged();

    if (m_pLastRenderModeAction != nullptr) {
        m_pLastRenderModeAction->setChecked(false);
    }

    updateAnimationBarVisibility(animationControlsEnabled);
}

void RenderView::presentGltfImport(gltf::Gltf* gltf)
{
    const bool animationControlsEnabled = gltf != nullptr && (model::gltf_operations::hasAnimations() || model::gltf_operations::hasEmitters());

    Actions::instance().setAnimationControlsEnabled(animationControlsEnabled);
    Actions::instance().updateAudioMuteState();
    Actions::instance().updateAnimationPopupState();

    updateAnimationBarVisibility(animationControlsEnabled);
}

void RenderView::presentSceneOpen(const GltfSceneOpenUseCase& useCase)
{
    if (useCase.pScene != nullptr) {
        m_currentSceneId = useCase.pScene->m_ID;
    }

    m_editModeCamera = CameraInfo();
    m_editModeCamera.cameraType = (toolButton_projection->text() == "Perspective") ? RenderViewState::PerspectiveCam : RenderViewState::OrthographicCam;

    m_renderModeCamera = CameraInfo();
    if (!openGLWidget->isEditMode()) {
        gatherCameraMenu();
    }

    stopAnimation();
}

void RenderView::presentSkinningEnabled(bool enabled)
{
    m_isSkinningEnabled = enabled;
}

void RenderView::presentMorphingEnabled(bool enabled)
{
    m_isMorphingEnabled = enabled;
}

void RenderView::updateCameraMenuEntry(const gltf::Node* pNode, const gltf::Camera* pCamera)
{
    comboBox_cameras->blockSignals(true);
    comboBox_cameras->removeAll(pNode);
    QString label = makeCameraMenuItemLabel(pNode, pCamera);
    int index = comboBox_cameras->setPair(label, pNode, pCamera);
    comboBox_cameras->blockSignals(false);

    if (m_renderModeCamera.nodeCameraPair.first == pNode) {
        comboBox_cameras->setCurrentIndex(index);
    }
}

void RenderView::fillViewportModeMenu()
{
    QMenu* pViewportModeMenu = new QMenu(pushButton_viewportMode);

    pViewportModeMenu->addAction(Actions::instance().editMode);
    pViewportModeMenu->addAction(Actions::instance().renderMode);

    pushButton_viewportMode->setMenu(pViewportModeMenu);
    connect(pViewportModeMenu, &QMenu::triggered, [this](QAction* action) { pushButton_viewportMode->setText(action->iconText()); });
    connect(Actions::instance().editMode, &QAction::triggered, [this] {
        setViewportMode(true);
        pushButton_RenderMode->setEnabled(true);
        pushButton_takeRender->setEnabled(false);
        toolButton_projection->setEnabled(true);
        toolButton_projection->setText(prevProjectionText);
        toolButton_projection->setToolTip(prevProjectionTooltip);
        toolButton_projection->setIcon(prevProjectionIcon);
        Actions::instance().setRenderModeActionsEnabled(true);
    });
    connect(Actions::instance().renderMode, &QAction::triggered, [this] {
        setViewportMode(false);
        pushButton_RenderMode->setEnabled(false);
        pushButton_takeRender->setEnabled(true);
        toolButton_projection->setEnabled(false);
        prevProjectionIcon = toolButton_projection->icon();
        prevProjectionTooltip = toolButton_projection->toolTip();
        prevProjectionText = toolButton_projection->text();
        toolButton_projection->setText("Perspective");
        toolButton_projection->setToolTip("Perspective");
        toolButton_projection->setIcon(QIcon(pathConstants::iconPerspectiveDisabled));
        Actions::instance().setRenderModeActionsEnabled(false);
    });
}

void RenderView::setViewportMode(bool isEditMode)
{
    // gatherCameraMenu implicitly overrides these so we just store them here
    CameraInfo lastRenderModeCamera = m_renderModeCamera;
    CameraInfo lastEditModeCamera = m_editModeCamera;

    openGLWidget->setEditMode(isEditMode);
    toolsWidget->setVisible(isEditMode);
    toolButton_grid->setVisible(isEditMode);
    comboBox_cameras->setVisible(!isEditMode);
    toolButton_gizmoSpace->setVisible(isEditMode);
    toolButton_projection->setVisible(isEditMode);

    Actions::instance().tool->setEnabled(isEditMode);
    Actions::instance().projection->setEnabled(isEditMode);

    if (isEditMode) {
        projectionChanged(lastEditModeCamera.cameraType);

        Actions::instance().createCameraFromView->setEnabled(true);

    } else {
        gatherCameraMenu();
        if (lastRenderModeCamera.cameraType == RenderViewState::RenderViewCam) {
            comboBox_cameras->setCurrentIndex(0);
            Actions::instance().createCameraFromView->setEnabled(true);

        } else if (lastRenderModeCamera.cameraType == RenderViewState::FixedCam) {
            int index = comboBox_cameras->indexOf(lastRenderModeCamera.nodeCameraPair);
            if (index > -1) {
                comboBox_cameras->setCurrentIndex(index);
            }
        } else {
            if (comboBox_cameras->count() > 1) {
                comboBox_cameras->setCurrentIndex(1);
            }
        }
    }
}

QString RenderView::makeCameraMenuItemLabel(const gltf::Node* pNode, const gltf::Camera* pCamera)
{
    std::string cameraName = pCamera != nullptr ? Name(*pCamera).forDisplay() : "NULL";
    std::string nodeName = pNode != nullptr ? Name(*pNode).forDisplay() : "NULL";

    std::string label = nodeName + " (" + cameraName + ")";
    return QString::fromStdString(label);
}

void RenderView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    pushButton_takeRender->setFixedWidth(pushButton_takeRender->fontMetrics().horizontalAdvance(pushButton_takeRender->text()) + 2 * constants::labelRightPadding + pushButton_takeRender->iconSize().width());
    pushButton_RenderMode->setFixedWidth(pushButton_RenderMode->fontMetrics().horizontalAdvance(pushButton_RenderMode->menu()->actions().front()->text())
        + pushButton_RenderMode->iconSize().width() + constants::pushButtonAsDropDownPadding);
    comboBox_cameras->setFixedWidth(toolsWidget->width() + 25);
}

void RenderView::updateAnimationBarVisibility(const bool animationsPresent)
{
    animationWidget->setVisible(animationsPresent);
    if (animationsPresent) {
        dockWidgetContents_top->setFixedHeight(56);
    } else {
        dockWidgetContents_top->setFixedHeight(28);
    }
}

// Animation

void RenderView::playAnimation()
{
    if (m_animationPlaying) {
        openGLWidget->playAudio();
    } else {
        openGLWidget->pauseAudio();
    }

    m_totalAnimationTime = openGLWidget->getGltfRenderLayer() != nullptr ? openGLWidget->getGltfRenderLayer()->getAnimationDuration() : 0.0f;
    horizontalSlider_animationTime->setMaximum(sliderTicksPerSecond * m_totalAnimationTime);
    toolButton_playPause->setBaseIcon(QIcon(m_animationPlaying ? pathConstants::iconTimelinePlay : pathConstants::iconTimelinePause));

    m_animationPlaying = !m_animationPlaying;
}

void RenderView::rewindAnimation()
{
    openGLWidget->stopAudio();

    m_currentAnimationTime = 0.0f;
    m_animationPlaying = false;
    horizontalSlider_animationTime->setSliderPosition(0);
    updateGltfAnimationTime();
}

void RenderView::forwardAnimation()
{
    openGLWidget->stopAudio();

    m_currentAnimationTime = m_totalAnimationTime;
    m_animationPlaying = false;
    horizontalSlider_animationTime->setSliderPosition(horizontalSlider_animationTime->maximum());
    updateGltfAnimationTime();
}

void RenderView::stopAnimation()
{
    openGLWidget->stopAudio();

    m_animationPlaying = false;
    m_currentAnimationTime = 0.0f;
    horizontalSlider_animationTime->setSliderPosition(0);
    openGLWidget->setAnimationTime({});
    model::ProjectManager::stateChangedTemporarily();
}

void RenderView::sliderMoved(int value)
{
    m_animationPlaying = false;
    openGLWidget->pauseAudio();
    m_currentAnimationTime = m_totalAnimationTime * static_cast<float>(value) / horizontalSlider_animationTime->maximum();
    if (std::isnan(m_currentAnimationTime)) {
        m_currentAnimationTime = 0.0f;
        openGLWidget->stopAudio();
    }
    updateGltfAnimationTime();
}

void RenderView::updateGltfAnimationTime()
{
    auto* renderLayer = openGLWidget->getGltfRenderLayer();
    if (renderLayer == nullptr) {
        return;
    }

    // currently we have to make sure animation is applied after the changes from the gltf module have been applied to the transfromhierarchy
    // therefore we can not apply the time directly to the animations because the values would be overwriten by GltfScene::updateScene
    m_totalAnimationTime = renderLayer != nullptr ? renderLayer->getAnimationDuration() : 0;
    horizontalSlider_animationTime->setMaximum(sliderTicksPerSecond * m_totalAnimationTime);

    if (m_totalAnimationTime >= 0) {
        openGLWidget->setAnimationTime(m_currentAnimationTime);
    } else {
        openGLWidget->setAnimationTime(std::nullopt);
    }
}

void RenderView::integrateTime(float deltaTime)
{
    if (!m_animationPlaying) {
        return;
    }

    m_currentAnimationTime += deltaTime;
    m_currentAnimationTime = std::fmod(m_currentAnimationTime, m_totalAnimationTime);
    if (std::isnan(m_currentAnimationTime)) {
        m_currentAnimationTime = 0.0f;
    }
    horizontalSlider_animationTime->setSliderPosition(sliderTicksPerSecond * m_currentAnimationTime);

    updateGltfAnimationTime();
}

void RenderView::updateRenderStateWireframe(bool isWireframe)
{
    // ensure last selected rendermode is reselected if wireframe is disabled
    if (m_pLastRenderModeAction == Actions::instance().visualizationModeWireframe) {
        m_pLastRenderModeAction = Actions::instance().visualizationModeDefaultShaded;
    }
    // get correct new action
    QAction* action = (isWireframe) ? Actions::instance().visualizationModeWireframe : m_pLastRenderModeAction;
    if (action == nullptr) {
        // if no action has been selected yet, m_pLastRenderModeAction (and this action) can be a nullptr. This prevents this case.
        action = Actions::instance().visualizationModeDefaultShaded;
    }

    // set selection state
    if (m_pLastRenderModeAction != nullptr) {
        m_pLastRenderModeAction->setChecked(false);
    }
    action->setChecked(true);

    setRenderModeButtonText(action);

    RenderMode newRenderMode;
    if (action == Actions::instance().visualizationModeDefaultShaded) {
        newRenderMode = RenderMode::None;
    } else if (action == Actions::instance().visualizationModeWireframe) {
        newRenderMode = RenderMode::Wireframe;
    } else {
        newRenderMode = static_cast<RenderMode>(action->data().toInt());
    }
    // set rendermode but dont emit update signal
    renderModeChanged(newRenderMode, false);
}

void RenderView::setRenderModeButtonText(QAction* action)
{
    auto text = pushButton_RenderMode->fontMetrics().elidedText(action->text(),
        Qt::TextElideMode::ElideRight, pushButton_RenderMode->width() - pushButton_RenderMode->iconSize().width() - constants::pushButtonAsDropDownPadding);
    pushButton_RenderMode->setText(text);
}
