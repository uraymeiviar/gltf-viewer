#include "widgets/GLTFRendererWidget.h"
#include "Actions.h"
#include "Clipboard.h"
#include "Constants.h"
#include "GltfPropertyPointer.h"
#include "Preferences.h"
#include "SelectionManager.h"
#include "filesystem/Path.h"
#include "helpers/QtFileSystem.h"
#include "interactors/GltfInteractor.h"
#include "interactors/LoadInteractor.h"
#include "interactors/RenderSettingInteractor.h"
#include "math/Math.h"
#include "model/ProjectManager.h"
#include "renderViewInput/GizmoEvent.h"
#include "renderViewInput/RenderViewInput.h"
#include "renderer/CommandGenerator.h"
#include "renderer/PickingRenderLayer.h"
#include "renderer/editor/AudioRenderer.h"
#include "useCases/GltfUseCases.h"
#include "widgets/renderContext/RendererContext.h"

#include <QAction>
#include <QFileDialog>
#include <QMouseEvent>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QTimer>
#include <QWindow>

#include <algorithm>

using namespace ux3d;
using namespace ux3d::editor;

GLTFRendererWidget::GLTFRendererWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_environmentMap(filesystem::Path(QtFileSystem::instance(), Preferences::getInstance()->getEnvironmentDirectory().toStdString()), this)
    , m_input(this, &m_cameraActor)
{
    LoadInteractor::getInstance()->registerOutput(this);
    GltfInteractor::getInstance()->registerOutput(this);
    RenderSettingInteractor::getInstance()->registerOutput(this);

    auto timer = new QTimer(this);
    timer->setInterval(1000 / constants::fps);
    connect(timer, &QTimer::timeout, [this] {
        update();
    });
    timer->start();

    setFocusPolicy(Qt::ClickFocus);
    // set minimum height to 1 to avoid crash bug with access to uninitialized m_rendererContext
    setMinimumHeight(1);

    installEventFilter(&m_input);

    connect(Preferences::getInstance(), &Preferences::iblMaxResolutionExpChanged, this, &GLTFRendererWidget::iblMaxResolutionExpChanged);
    connect(Preferences::getInstance(), &Preferences::msaaExpChanged, [this](int msaaExponent) {
        if (m_rendererContext != nullptr) {
            m_rendererContext->renderParameters.multiSampleCount = msaaExponent <= 0 ? 0 : 1 << msaaExponent;
        }
    });

    connect(Actions::instance().renderCapture, &QAction::triggered, this, &GLTFRendererWidget::renderCapture);
    connect(Actions::instance().createCameraFromView, &QAction::triggered, this, &GLTFRendererWidget::createGLTFCameraFromFreeCamera);
    connect(Actions::instance().cameraOrientationTop, &QAction::triggered, [this] {
        orientUserCamera(CameraOrientation::Top);
    });
    connect(Actions::instance().cameraOrientationBottom, &QAction::triggered, [this] {
        orientUserCamera(CameraOrientation::Bottom);
    });
    connect(Actions::instance().cameraOrientationRight, &QAction::triggered, [this] {
        orientUserCamera(CameraOrientation::Right);
    });
    connect(Actions::instance().cameraOrientationLeft, &QAction::triggered, [this] {
        orientUserCamera(CameraOrientation::Left);
    });
    connect(Actions::instance().cameraOrientationFront, &QAction::triggered, [this] {
        orientUserCamera(CameraOrientation::Front);
    });
    connect(Actions::instance().cameraOrientationBack, &QAction::triggered, [this] {
        orientUserCamera(CameraOrientation::Back);
    });

    connect(Actions::instance().viewportResolution, &QAction::triggered, this, &GLTFRendererWidget::createViewportResolutionPopup);
}

GLTFRendererWidget::~GLTFRendererWidget()
{
    makeCurrent();

    LoadInteractor::getInstance()->unregisterOutput(this);
    GltfInteractor::getInstance()->unregisterOutput(this);
    RenderSettingInteractor::getInstance()->unregisterOutput(this);

    removeEventFilter(&m_input);

    doneCurrent();
}

const gltf::Scene* GLTFRendererWidget::getCurrentScene() const
{
    return m_rendererContext->renderParameters.pActiveGltfScene;
}

void GLTFRendererWidget::createViewportResolutionPopup()
{
    core::logInfo("Not implemented yet.");
}

void GLTFRendererWidget::renderCapture()
{
    bool isSavingAllowed = true;
    if (!isSavingAllowed) {
        return;
    }

    static QString captureDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString capturePath = captureDirectory + "/";

    {
        // TODO: When C++20 is available, use the std::chrono calendar implementation instead of the C API
        char timestamp[64];
        time_t currentTime;
        time(&currentTime);
        auto timeInfo = localtime(&currentTime);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H'%M'%S", timeInfo);
        capturePath.append("Capture ");
        capturePath.append(QString(timestamp));
    }

    QFileDialog dialog(this, "Render Capture", capturePath);
    dialog.setMimeTypeFilters({ "image/png", "image/jpeg" });
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setLabelText(QFileDialog::Accept, "Capture");

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (dialog.selectedFiles().empty()) {
        return;
    }

    // Use the same directory next time
    captureDirectory = dialog.directory().path();

    CaptureConfiguration configuration;
    configuration.gltfSceneIndex = m_rendererContext->renderParameters.activeGltfSceneID;
    configuration.fitUserCameraToScene = false;
    configuration.outputFile = dialog.selectedFiles().front().toStdString();
    configuration.renderQuality = 90;
    configuration.width = 0;
    configuration.height = 0;
    configuration.numberOfFrames = 1;
    if (!m_rendererContext->renderParameters.cubeMapEnabled) {
        configuration.clearColor = m_rendererContext->renderParameters.clearColor;
    }

    if (m_rendererContext->renderParameters.animationTime.has_value() && *m_rendererContext->renderParameters.animationTime > 0) {
        configuration.framerate = 1000;
        configuration.animationEnabled = true;
        configuration.startFrameIndex = *m_rendererContext->renderParameters.animationTime * configuration.framerate;
    } else {
        configuration.animationEnabled = false;
    }

    queueCapture(configuration);
}

void GLTFRendererWidget::presentGltfOpen(gltf::Gltf* pGltf)
{
    core::debugLog(core::LogLevel::Info, "GLTFRendererWidget: Initializing scene renderer");

    model::ProjectManager::meshVisualizationChanged();

    makeCurrent();

    m_cameraActor.setCamera(m_rendererContext->getRenderState().gltfLayer->addCamera());

    if (Actions::instance().projectionPerspective->isChecked()) {
        getCameraActor().changeUserCameraType(renderer::Camera::Type::Perspective);
    } else if (Actions::instance().projectionOrthographic->isChecked()) {
        getCameraActor().changeUserCameraType(renderer::Camera::Type::Orthographic);
    } else {
        Actions::instance().projectionPerspective->trigger();
    }

    useUserCamera();

    m_rendererContext->renderParameters.materialVariantID = std::nullopt;
    m_rendererContext->renderParameters.pActiveGltf = pGltf;

    m_rendererContext->getRenderState().gltfLayer->updateGltf(*m_rendererContext->renderParameters.pActiveGltf);

    setRenderMode(editor::RenderMode::None);

    core::debugLog(core::LogLevel::Info, "GLTFRendererWidget: Scene renderer initialized");
    m_cameraNeedsUpdate = true;
}

void GLTFRendererWidget::presentGltfReloaded(gltf::Gltf* pGltf)
{

    if (m_cameraNeedsUpdate) {
        GltfSceneOpenUseCase useCase;
        GltfInteractor::getInstance()->execute(useCase);
        m_cameraNeedsUpdate = false;
    }
    makeCurrent();

    m_rendererContext->renderParameters.pActiveGltf = pGltf;

    // Use the already opened scene
    m_rendererContext->renderParameters.pActiveGltfScene = nullptr;
    if (m_rendererContext->renderParameters.activeGltfSceneID >= 0) {
        m_rendererContext->renderParameters.pActiveGltfScene = m_rendererContext->renderParameters.pActiveGltf->getByID<gltf::Scene>(
            static_cast<uint32_t>(m_rendererContext->renderParameters.activeGltfSceneID));
    }
    if (m_rendererContext->renderParameters.pActiveGltfScene == nullptr) {
        GltfSceneOpenUseCase useCase;
        GltfInteractor::getInstance()->execute(useCase);
    }

    m_rendererContext->getRenderState().gltfLayer->updateGltf(*m_rendererContext->renderParameters.pActiveGltf);

    if (m_rendererContext->renderParameters.pActiveGltfScene != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->updateScene(
            *m_rendererContext->renderParameters.pActiveGltf, *m_rendererContext->renderParameters.pActiveGltfScene);
    }

    gltf::Node* activeCameraNode = nullptr;
    if (m_rendererContext->renderParameters.activeGltfCameraNodeId >= 0) {
        activeCameraNode = m_rendererContext->renderParameters.pActiveGltf->getByID<gltf::Node>(
            static_cast<uint32_t>(m_rendererContext->renderParameters.activeGltfCameraNodeId));
    }

    if (activeCameraNode != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->setActiveGltfCameraByNode(activeCameraNode);
    } else {
        useUserCamera();
    }

    m_rendererContext->getRenderState().editorLayer->update();

    // this must happen late, as the transform hierarchy is applied in this step
    m_rendererContext->getRenderState().gltfLayer->applyAnimationTime(m_rendererContext->renderParameters.animationTime);
}

void GLTFRendererWidget::presentSceneOpen(const GltfSceneOpenUseCase& useCase)
{
    m_rendererContext->renderParameters.pActiveGltfScene = useCase.pScene;

    if (m_rendererContext->renderParameters.pActiveGltfScene != nullptr) {
        m_rendererContext->renderParameters.activeGltfSceneID = m_rendererContext->renderParameters.pActiveGltfScene->m_ID;
        m_cameraActor.fitCameraToScene(*m_rendererContext->renderParameters.pActiveGltfScene);

        m_rendererContext->getRenderState().gltfLayer->updateScene(
            *m_rendererContext->renderParameters.pActiveGltf, *m_rendererContext->renderParameters.pActiveGltfScene);
    } else {
        // ensure the camera extents are set to some value even if the viewport is empty
        m_cameraActor.fitCameraToScene(gltf::Scene(0));
    }
}

void GLTFRendererWidget::presentIBLEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().commandGenerator->setIBLEnabled(enabled);
        m_rendererContext->renderParameters.iblEnabled = enabled;
    }
}

void GLTFRendererWidget::presentCubeMapEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->enableCubeMap(enabled);
        m_rendererContext->renderParameters.cubeMapEnabled = enabled;
    }
}

void GLTFRendererWidget::presentPunctualLightsEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().commandGenerator->setPunctualLightsEnabled(enabled);
        m_rendererContext->renderParameters.punctualLightsEnabled = enabled;
    }
}

void GLTFRendererWidget::presentToneMappingChanged(renderer::ToneMappingsEnum toneMapping)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().commandGenerator->setToneMapping(toneMapping);
        m_rendererContext->renderParameters.toneMapping = toneMapping;
    }
}

void GLTFRendererWidget::presentExposureChanged(float exposure)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().commandGenerator->setExposure(exposure);
        m_rendererContext->renderParameters.exposure = exposure;
    }
}

void GLTFRendererWidget::presentSkinningEnabled(bool enable)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableSkinning = enable;
        m_rendererContext->renderParameters.isSkinningEnabled = enable;
    }
}

void GLTFRendererWidget::presentMorphingEnabled(bool enable)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableMorphing = enable;
        m_rendererContext->renderParameters.isMorphingEnabled = enable;
    }
}

void GLTFRendererWidget::presentPbrSpecularGlossinessEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableSpecularGlossiness = enabled;
        m_rendererContext->renderParameters.isPbrSpecularGlossinessEnabled = enabled;
    }
}

void GLTFRendererWidget::presentUnlitEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableUnlit = enabled;
        m_rendererContext->renderParameters.isUnlitEnabled = enabled;
    }
}

void GLTFRendererWidget::presentClearCoatEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableClearCoat = enabled;
        m_rendererContext->renderParameters.isClearCoatEnabled = enabled;
    }
}

void GLTFRendererWidget::presentSheenEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableSheen = enabled;
        m_rendererContext->renderParameters.isSheenEnabled = enabled;
    }
}

void GLTFRendererWidget::presentTransmissionEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableTransmission = enabled;
        m_rendererContext->renderParameters.isTransmissionEnabled = enabled;
    }
}

void GLTFRendererWidget::presentVolumeEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableVolume = enabled;
        m_rendererContext->renderParameters.isVolumeEnabled = enabled;
    }
}

void GLTFRendererWidget::presentIridescenceEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableIridescence = enabled;
        m_rendererContext->renderParameters.isIridescenceEnabled = enabled;
    }
}

void GLTFRendererWidget::presentIOREnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableIOR = enabled;
        m_rendererContext->renderParameters.isIOREnabled = enabled;
    }
}

void GLTFRendererWidget::presentSpecularEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableSpecular = enabled;
        m_rendererContext->renderParameters.isSpecularEnabled = enabled;
    }
}

void GLTFRendererWidget::presentEmissiveStrengthEnabled(bool enabled)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->renderSettings.enableEmissiveStrength = enabled;
        m_rendererContext->renderParameters.isEmissiveStrengthEnabled = enabled;
    }
}

void GLTFRendererWidget::presentEnvironmentMapRotation(float rotation)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().commandGenerator->setEnvironmentMapRotation(rotation);
        m_rendererContext->renderParameters.envMapRotation = rotation;
    }
}

void GLTFRendererWidget::presentEnvironmentMapBlurNormalized(float blur)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().commandGenerator->setEnvironmentMapBlurNormalized(blur);
        m_rendererContext->renderParameters.envMapBlur = blur;
    }
}

void GLTFRendererWidget::presentEnvironmentMapIntensity(float value)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().commandGenerator->setEnvironmentMapIntensity(value);
        m_rendererContext->renderParameters.envMapIntensity = value;
    }
}

void GLTFRendererWidget::presentMaterialVariantsSelected(std::optional<uint32_t> _materialVariantID)
{
    if (m_rendererContext != nullptr && m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->renderParameters.materialVariantID = _materialVariantID;
    }
}

void GLTFRendererWidget::presentEnvironmentMapPath(const filesystem::Path& path)
{
    if (m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->setEnvironmentMap(path);
        m_rendererContext->renderParameters.envMapPath = path;
    }
}

void GLTFRendererWidget::initializeGL()
{
    m_rendererContext = std::make_unique<RendererContext>();
    m_rendererContext->initGL(false); // do not initialize the audio renderer. it is not needed currently

    m_rendererContext->getRenderState().gltfLayer->iblMaxResolution = 1 << Preferences::getInstance()->getIblMaxResolutionExp();
    m_environmentMap.refresh();

    int multiSampleCount = 1 << std::max(0, Preferences::getInstance()->getMSAAExp());
    m_rendererContext->renderParameters.multiSampleCount = multiSampleCount <= 1 ? 0 : multiSampleCount;
}

double GLTFRendererWidget::getDevicePixelRatio() const
{
    return window()->windowHandle()->devicePixelRatio();
}

uint32_t GLTFRendererWidget::getWidth() const
{
    return m_rendererContext->renderParameters.width;
}

uint32_t GLTFRendererWidget::getHeight() const
{
    return m_rendererContext->renderParameters.height;
}

void GLTFRendererWidget::paintGL()
{
    emit rendererReady();
    if (!model::ProjectManager::getInstance()->isRendererEnabled()) {
        return;
    }

    if (!isValid()) {
        // isValid(): Returns true if the window's OpenGL resources, like the context, have been successfully initialized.
        // Note that the return value is always false until the window becomes exposed (shown).
        return;
    }

    float deltaTime = 0.0f;
    if (m_frameTime == Clock::time_point()) {
        m_frameTime = Clock::now();
    } else {
        auto frameTime = Clock::now();
        deltaTime = static_cast<float>((frameTime - m_frameTime).count()) * Clock::time_point::period::num / Clock::time_point::period::den;
        m_frameTime = frameTime;
        emit integrateTime(deltaTime);

        // Integrate motion of user camera
        m_input.transformCamera(deltaTime);
    }

    makeCurrent();
    context()->functions()->glClearColor(0.1, 0.1, 0.1, 0);
    context()->functions()->glClear(GL_COLOR_BUFFER_BIT);

    double availableWidth = width() * getDevicePixelRatio();
    double availableHeight = height() * getDevicePixelRatio();

    // Set default aspect ratio. The user camera and glTF -> render camera will use that one, unless a glTF cam specs its own aspect ratio.
    if (m_useViewportResolution && m_viewportResolution.has_value()) {
        m_rendererContext->getRenderState().gltfLayer->viewportAspectRatio = std::get<0>(*m_viewportResolution) / std::get<1>(*m_viewportResolution);
    } else {
        m_rendererContext->getRenderState().gltfLayer->viewportAspectRatio = availableWidth / availableHeight;
    }

    // Update glTF -> render cams if a glTF cam is active
    if (m_rendererContext->renderParameters.activeGltfCameraNodeId >= 0 && m_rendererContext->renderParameters.pActiveGltfScene != nullptr) {
        auto scene = m_rendererContext->renderParameters.pActiveGltfScene;
        m_rendererContext->getRenderState().gltfLayer->updateCameras(scene->getNodes());
    }

    // Also, update the user camera
    if (m_cameraActor.getUserCamera() != nullptr) {
        m_cameraActor.getUserCamera()->setAspectRatio(m_rendererContext->getRenderState().gltfLayer->viewportAspectRatio);
    }

    double width, height;

    // Now, the active render cam aspect ratio must be enforced
    if (auto* activeCamera = m_rendererContext->getRenderState().gltfLayer->getActiveCamera();
        activeCamera != nullptr) {
        double enforcedAspectRatio = activeCamera->getAspectRatio();
        double surfaceAspectRatio = availableWidth / availableHeight;
        double aspectRatioRatio = surfaceAspectRatio / enforcedAspectRatio;

        if (aspectRatioRatio > 1) {
            width = availableWidth / aspectRatioRatio;
            height = availableHeight;
        } else if (aspectRatioRatio < 1) {
            width = availableWidth;
            height = availableHeight * aspectRatioRatio;
        } else {
            width = availableWidth;
            height = availableHeight;
        }
    } else {
        width = availableWidth;
        height = availableHeight;
    }

    double horizontalOffset = std::round((availableWidth - width) / 2.0);
    double verticalOffset = std::round((availableHeight - height) / 2.0);

    // Ensure the extent is valid. Rounding instead of truncating prevents flickering due to floating-point errors.
    width = std::max(1.0, std::round(width));
    height = std::max(1.0, std::round(height));

    if (auto* activeCamera = m_rendererContext->getRenderState().gltfLayer->getActiveCamera();
        activeCamera != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->getActiveCamera()->setViewportResolution(width, height);
    }
    m_rendererContext->renderParameters.width = width;
    m_rendererContext->renderParameters.height = height;

    if (!m_renderCaptureQueue.empty()) {
        auto configuration = m_renderCaptureQueue.front();
        if (configuration.width == 0 && configuration.height == 0) {
            configuration.width = width;
            configuration.height = height;
            if (m_useViewportResolution && m_viewportResolution.has_value()) {
                auto tuple = m_viewportResolution.value();
                configuration.width = std::get<0>(tuple);
                configuration.height = std::get<1>(tuple);
            }
        }
        capture(configuration);
        m_renderCaptureQueue.pop();
    }

    m_rendererContext->paintGL(deltaTime);
    performPicking();

    QRect sourceRect(0, 0, m_rendererContext->renderParameters.width, m_rendererContext->renderParameters.height);
    QRect targetRect(horizontalOffset, verticalOffset, m_rendererContext->renderParameters.width, m_rendererContext->renderParameters.height);

    QOpenGLFramebufferObject::blitFramebuffer(nullptr, targetRect, m_rendererContext->m_renderState.framebuffer, sourceRect, GL_COLOR_BUFFER_BIT);
}

void GLTFRendererWidget::updateEnvironmentMap(const model::EnvironmentMapBinding& environmentMapBinding)
{
    m_environmentMap = environmentMapBinding;
    m_environmentMap.bind(this);
    m_environmentMap.refresh();
}

void GLTFRendererWidget::iblMaxResolutionExpChanged()
{
    m_rendererContext->getRenderState().gltfLayer->iblMaxResolution = 1 << Preferences::getInstance()->getIblMaxResolutionExp();
}

void GLTFRendererWidget::setRenderMode(RenderMode renderMode)
{
    m_rendererContext->renderParameters.renderMode = renderMode;
    if (m_rendererContext->getRenderState().editorLayer != nullptr) {
        m_rendererContext->getRenderState().editorLayer->m_renderMode = renderMode;
    }
}

void GLTFRendererWidget::setShowBoundingBoxes(bool show)
{
    if (m_rendererContext->getRenderState().editorLayer != nullptr) {
        m_rendererContext->getRenderState().editorLayer->showBoundingBoxes = show;
    }
}

void GLTFRendererWidget::setShowSkeletons(bool show)
{
    if (m_rendererContext->getRenderState().editorLayer != nullptr) {
        m_rendererContext->getRenderState().editorLayer->showSkinsAndBones = show;
    }
}

void GLTFRendererWidget::useGltfCameraByNode(const gltf::Node* pNode)
{
    if (pNode->getCamera() != nullptr) {
        m_rendererContext->getRenderState().gltfLayer->setActiveGltfCameraByNode(pNode);
        m_rendererContext->renderParameters.activeGltfCameraNodeId = pNode->m_ID;
        m_input.setRenderViewState(RenderViewState::FixedCam);
        RenderSettingInteractor::getInstance()->cameraTypeChanged(
            pNode->getCamera()->getType() == gltf::Camera::ProjectionType::Orthographic);
    }
}

void GLTFRendererWidget::orientUserCamera(CameraOrientation orientation)
{
    switch (orientation) {
    case CameraOrientation::Top:
        m_cameraActor.setCameraRotation(math::pi<float>(), math::pi<float>() / 2.f);
        break;
    case CameraOrientation::Bottom:
        m_cameraActor.setCameraRotation(math::pi<float>(), -math::pi<float>() / 2.f);
        break;
    case CameraOrientation::Right:
        m_cameraActor.setCameraRotation(-math::pi<float>() / 2.f, 0.f);
        break;
    case CameraOrientation::Left:
        m_cameraActor.setCameraRotation(math::pi<float>() / 2.f, 0.f);
        break;
    case CameraOrientation::Front:
        m_cameraActor.setCameraRotation(0.f, 0.f);
        break;
    case CameraOrientation::Back:
        m_cameraActor.setCameraRotation(math::pi<float>(), 0.f);
        break;
    }
}

void GLTFRendererWidget::useUserCamera()
{
    if (m_rendererContext->getRenderState().gltfLayer == nullptr || m_cameraActor.getUserCamera() == nullptr) {
        core::logError("Unable to use user camera");
        return;
    }

    if (m_cameraActor.getUserCamera()->getType() == renderer::Camera::Type::Perspective) {
        m_input.setRenderViewState(RenderViewState::PerspectiveCam);
        RenderSettingInteractor::getInstance()->cameraTypeChanged(false);
    } else {
        m_input.setRenderViewState(RenderViewState::OrthographicCam);
        RenderSettingInteractor::getInstance()->cameraTypeChanged(true);
    }

    m_rendererContext->getRenderState().gltfLayer->setActiveCamera(m_cameraActor.getUserCamera());
    m_rendererContext->renderParameters.activeGltfCameraNodeId = -1;
    m_cameraActor.updateUserCameraTransform();
}

void GLTFRendererWidget::createGLTFCameraFromFreeCamera()
{
    if (m_cameraActor.getUserCamera() == nullptr) {
        core::logError("glTF camera could not be created from user camera.");
        return;
    }

    const math::vec3 translation = m_cameraActor.getUserCamera()->getPositon();
    const math::vec3 up(0, 1, 0);
    const math::quat rotation = math::quatLookAt(m_cameraActor.getUserCamera()->getView(), up);

    auto taskDescription = nlohmann::json({ { "create_camera_from_view", nlohmann::json::object() } });
    taskDescription["create_camera_from_view"]["zNear"] = m_cameraActor.getUserCamera()->getZNearPlane();
    taskDescription["create_camera_from_view"]["zFar"] = m_cameraActor.getUserCamera()->getZFarPlane();
    taskDescription["create_camera_from_view"]["yFov"] = m_cameraActor.getUserCamera()->getYFoV();
    taskDescription["create_camera_from_view"]["translation"] = nlohmann::json::array({ translation.x, translation.y, translation.z });
    taskDescription["create_camera_from_view"]["rotation"] = nlohmann::json::array({ rotation.x, rotation.y, rotation.z, rotation.w });
    if (m_rendererContext->renderParameters.pActiveGltfScene != nullptr) {
        GltfPropertyConstPointer scenePointer(m_rendererContext->renderParameters.pActiveGltfScene);
        nlohmann::json::json_pointer sceneJsonPointer = gltfPointerToJsonPointer(scenePointer, GltfPropertyType::Scenes);
        taskDescription["create_camera_from_view"]["sceneIdx"] = std::stoi(sceneJsonPointer.back());
    } else {
        taskDescription["create_camera_from_view"]["sceneIdx"] = -1;
    }

    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    auto promise = model::ProjectManager::getInstance()->model.schedule(taskDescription);
    promise.error.observe_on(uiWorker).subscribe([](const std::string& e) {
        core::logError(e.c_str());
    });
    promise.success.observe_on(uiWorker).subscribe([](const std::string& e) {
        size_t cameraIndex = model::ProjectManager::getInstance()->getGltf().getCameras().size() - 1;
        Clipboard::instance().clearAndSetSelectionPath(QString::fromStdString("/gltf/cameras/" + std::to_string(cameraIndex)));
    });
}

void GLTFRendererWidget::setAnimationTime(const std::optional<float>& time)
{
    m_rendererContext->renderParameters.animationTime = time;
}

renderer::GltfRenderLayer* GLTFRendererWidget::getGltfRenderLayer() const
{
    return m_rendererContext->getRenderState().gltfLayer;
}

void GLTFRendererWidget::performPicking()
{
    if (m_rendererContext->getRenderState().pickingLayer == nullptr) {
        return;
    }
    // download picking texture:
    m_pickingBuffer = m_rendererContext->getRenderState().pickingFramebuffer->toImage(false);

    QRgb pixelValue = 0;

    // Picking position
    {
        auto [pickingX, pickingY] = m_input.getLastMousePos();
        if (!m_isPicking) {
            auto pos = m_input.getCurrentMousePos();
            pickingX = pos.x;
            pickingY = pos.y;
        }

        const double devicePixelRatio = devicePixelRatioF();
        pickingX *= devicePixelRatio;
        pickingY *= devicePixelRatio;

        pickingY = m_rendererContext->renderParameters.height - pickingY - 1;

        if (pickingY >= 0 && pickingY < m_pickingBuffer.height() && pickingX >= 0 && pickingX < m_pickingBuffer.width()) {
            pixelValue = m_pickingBuffer.pixel(QPoint(pickingX, pickingY)) & 0x00FFFFFF;
        }
    }

    // evaluate picked data

    renderer::Mesh* mesh = m_rendererContext->getRenderState().pickingLayer->getMesh(pixelValue);

    bool pickedGizmoMesh = false;

    // Orientation Gizmo face picking & setting interaction states
    for (auto& [piece, state] : m_rendererContext->getRenderState().editorLayer->m_orientationGizmoPieces) {
        // Reset the color of all orientation gizmo pieces
        state = renderer::OrientationCubeInteractionMode::Default;
    }
    if (auto orientationGizmoPiece = m_rendererContext->getRenderState().editorLayer->getEditorResourceManager().mapOrientationGizmoMeshToPiece(mesh); orientationGizmoPiece.has_value()) {
        if (m_isPicking) {
            // set the color of the gizmo to "selected"
            m_rendererContext->getRenderState().editorLayer->m_orientationGizmoPieces[orientationGizmoPiece.value()] = renderer::OrientationCubeInteractionMode::Selected;

            // set the camera orientation depending on the gizmo
            switch (orientationGizmoPiece.value()) {
            case renderer::OrientationGizmoPiece::Top:
                orientUserCamera(CameraOrientation::Top);
                break;
            case renderer::OrientationGizmoPiece::Bottom:
                orientUserCamera(CameraOrientation::Bottom);
                break;
            case renderer::OrientationGizmoPiece::Front:
                orientUserCamera(CameraOrientation::Front);
                break;
            case renderer::OrientationGizmoPiece::Back:
                orientUserCamera(CameraOrientation::Back);
                break;
            case renderer::OrientationGizmoPiece::Right:
                orientUserCamera(CameraOrientation::Right);
                break;
            case renderer::OrientationGizmoPiece::Left:
                orientUserCamera(CameraOrientation::Left);
                break;
            default:
                break;
            }
            pickedGizmoMesh = true;
        } else {
            // set the color of the gizmo to hover
            m_rendererContext->getRenderState().editorLayer->m_orientationGizmoPieces[orientationGizmoPiece.value()] = renderer::OrientationCubeInteractionMode::Hover;
        }
    }

    if (m_rendererContext->getRenderState().editorLayer->hasActiveGizmo()) {
        if (auto transformMode = m_rendererContext->getRenderState().editorLayer->getEditorResourceManager().mapGizmoMeshToTransformMode(mesh);
            m_gizmoState.interactionMode == renderer::GizmoInteractionMode::Hover && transformMode.has_value()) {
            m_gizmoState.interactionMode = (m_isPicking) ? renderer::GizmoInteractionMode::Drag : renderer::GizmoInteractionMode::Hover;
            m_gizmoState.transformMode = *transformMode;
            pickedGizmoMesh = true;
        } else if (m_isPicking || m_gizmoState.interactionMode == renderer::GizmoInteractionMode::Hover) {
            m_gizmoState.interactionMode = renderer::GizmoInteractionMode::Hover;
            switch (m_input.getInputToolState()) {
            case InputToolState::Translation:
                m_gizmoState.transformMode = renderer::GizmoTransformMode::DefaultTranslate;
                break;
            case InputToolState::Scaling:
                m_gizmoState.transformMode = renderer::GizmoTransformMode::DefaultScale;
                break;
            case InputToolState::Rotation:
                m_gizmoState.transformMode = renderer::GizmoTransformMode::DefaultRotate;
                break;
            default:
                break;
            }
            pickedGizmoMesh = false;
        }
    }

    if (!pickedGizmoMesh && m_isPicking) {
        SelectionInfo selectionInfo;

        const ux3d::renderer::ReverseMapping& reverseMapping = m_rendererContext->getReverseMapping();
        if (reverseMapping.size() == 0) {
            selectionInfo.type = GltfPropertyType::None;
        } else {
            if (
                auto pickedNodeIterator = reverseMapping.find(mesh);
                pickedNodeIterator != reverseMapping.end()) {
                selectionInfo.type = GltfPropertyType::Nodes;
                selectionInfo.pointer = const_cast<gltf::Node*>(pickedNodeIterator->second);
                switch (m_input.getInputToolState()) {
                case InputToolState::Translation:
                    m_gizmoState.transformMode = renderer::GizmoTransformMode::DefaultTranslate;
                    break;
                case InputToolState::Scaling:
                    m_gizmoState.transformMode = renderer::GizmoTransformMode::DefaultScale;
                    break;
                case InputToolState::Rotation:
                    m_gizmoState.transformMode = renderer::GizmoTransformMode::DefaultRotate;
                    break;
                default:
                    break;
                }
            } else {
                selectionInfo.type = GltfPropertyType::None;
            }
        }
        selectionInfo.source = SelectionSource::Renderview;
        std::vector<SelectionInfo> selections = { selectionInfo };
        emit SelectionManager::getInstance()->selectObjects(selections);
    } else {
        GizmoEvent event(m_gizmoState);
        QCoreApplication::sendEvent(this, &event);
    }

    m_isPicking = false;
}

void GLTFRendererWidget::queueCapture(const CaptureConfiguration& configuration)
{
    m_renderCaptureQueue.push(configuration);
}

void GLTFRendererWidget::capture(const CaptureConfiguration& configuration)
{
    bool renderQueueIsBusy = m_renderCaptureQueue.size() > 1;

    if (configuration.height <= 0 && configuration.width <= 0) {
        core::logError("Width or height are required");
        configuration.callbackError(false);
        return;
    }

    if (m_rendererContext->renderParameters.pActiveGltfScene == nullptr) {
        core::logInfo("No scene opened. Cannot create rendering.");
        configuration.callbackError(renderQueueIsBusy);
        return;
    }

    // get the originally used camera to copy its values and reset it after rendering
    renderer::Camera* originalCamera = nullptr;
    if (m_rendererContext->getRenderState().gltfLayer != nullptr) {
        originalCamera = m_rendererContext->getRenderState().gltfLayer->getActiveCamera();
    }

    if (originalCamera == nullptr) {
        core::logError("Original camera could not be found");
        configuration.callbackError(false);
        return;
    }

    // setup a camera and cameraActor for this rendering
    renderer::Camera camera = *originalCamera;
    CameraActor cameraActor = CameraActor();
    cameraActor.setCamera(&camera);

    RendererContext screenCapContext;
    screenCapContext.initGL(false);
    screenCapContext.dirtyStateChanged();

    // copy values of original context at first
    screenCapContext.renderParameters = m_rendererContext->renderParameters;

    // use the camera in the new render context
    screenCapContext.getRenderState().gltfLayer->setActiveCamera(&camera);

    // apply gestaltor preferences
    screenCapContext.getRenderState().gltfLayer->iblMaxResolution = 1 << Preferences::getInstance()->getIblMaxResolutionExp();
    const int32_t msaaExponent = Preferences::getInstance()->getMSAAExp();
    if (msaaExponent <= 0) {
        screenCapContext.renderParameters.multiSampleCount = 0;
    } else {
        screenCapContext.renderParameters.multiSampleCount = pow(2, msaaExponent);
    }

    // do not render gizmos and grid
    screenCapContext.renderParameters.drawHelpers = false;

    // override render parameters with user specified options
    if (configuration.toneMapping.has_value()) {
        screenCapContext.renderParameters.toneMapping = configuration.toneMapping.value();
    }
    if (configuration.clearColor.has_value()) {
        screenCapContext.renderParameters.clearColor = configuration.clearColor.value();
        screenCapContext.renderParameters.cubeMapEnabled = false;
    } else {
        screenCapContext.renderParameters.cubeMapEnabled = true;
    }
    if (configuration.environmentMapPath.isFile()) {
        // only set actual environment maps, if none is defined use the default one / currently used one
        screenCapContext.renderParameters.envMapPath = configuration.environmentMapPath;
    }

    screenCapContext.setGltfRenderLayerParamsAndComGenFromParameters();

    // update renderer gltf objects
    gltf::Gltf* gltf = m_rendererContext->renderParameters.pActiveGltf;
    screenCapContext.getRenderState().gltfLayer->updateGltf(*gltf);

    // set requested scene by ID
    screenCapContext.renderParameters.activeGltfSceneID = configuration.gltfSceneIndex;

    // coming from the cli we want to fit the user camera to the scene
    if (configuration.fitUserCameraToScene) {
        bool sceneFound = false;
        for (gltf::Scene& scene : gltf->getScenes()) {
            if (scene.m_ID == screenCapContext.renderParameters.activeGltfSceneID) {
                screenCapContext.renderParameters.pActiveGltfScene = &scene;
                cameraActor.fitCameraToScene(*screenCapContext.renderParameters.pActiveGltfScene);

                core::logInfo("Scene selected: %d %s", screenCapContext.renderParameters.activeGltfSceneID, scene.getName().c_str());
                sceneFound = true;
            }
        }
        if (false == sceneFound) {
            core::logError("Scene not found: %d", configuration.gltfSceneIndex);
            configuration.callbackError(false);
            return;
        }
    }

    if (configuration.radius.has_value()) {
        cameraActor.setZoom(configuration.radius.value());
        cameraActor.updateUserCameraTransform();
    }
    if (configuration.target.has_value()) {
        cameraActor.setTarget(configuration.target.value());
        cameraActor.updateUserCameraTransform();
    }
    if (configuration.theta.has_value() || configuration.phi.has_value()) {
        cameraActor.setCameraRotation(math::radians(-configuration.theta.value_or(0.0f)), math::radians(configuration.phi.value_or(0.0f)));
    }

    if (configuration.vFoV.has_value()) {
        camera.setPerspective(camera.getAspectRatio(), math::radians(configuration.vFoV.value()), camera.getZNearPlane(), camera.getZFarPlane());
    }

    // update renderer Scene
    gltf::Scene* scene = screenCapContext.renderParameters.pActiveGltfScene;
    screenCapContext.getRenderState().gltfLayer->updateScene(*gltf, *scene);

    // search for camera node by specified camera ID and set respective renderer::camera
    int32_t glTFCameraID = -1;

    if (configuration.gltfCameraIndex.has_value()) {
        if (gltf->getCameras().size() > configuration.gltfCameraIndex.value()) {
            glTFCameraID = gltf->getCameras().at(configuration.gltfCameraIndex.value()).m_ID;
        } else { // invalid camera index
            core::logError("Camera ID not found: %d", configuration.gltfCameraIndex.value());
            configuration.callbackError(false);
            return;
        }
    }

    gltf::Camera* gltfCamera = nullptr;

    // Find the correct glTF camera if the configuration specifies one
    if (screenCapContext.renderParameters.activeGltfCameraNodeId >= 0) { // non-cli case where a glTF camera is currently in use
        gltfCamera = gltf->getNodes().at(screenCapContext.renderParameters.activeGltfCameraNodeId).getCamera();

        if (gltfCamera == nullptr) {
            core::logError("Active camera could not be retrieved");
            configuration.callbackError(false);
            return;
        }
    } else {
        if (glTFCameraID >= 0) { // only used if a glTF camera is specified on the cli
            for (gltf::Node& node : gltf->getNodes()) {
                if (node.getCamera() == nullptr) {
                    continue;
                }

                if (node.getCamera()->m_ID == glTFCameraID) {
                    screenCapContext.getRenderState().gltfLayer->setActiveGltfCameraByNode(&node);

                    // now fetch renderer::Camera
                    renderer::Camera* gltfRendererCamera = screenCapContext.getRenderState().gltfLayer->getActiveCamera();
                    if (gltfRendererCamera == nullptr) {
                        core::logError("Active camera could not be retrieved");
                        configuration.callbackError(false);
                        return;
                    }

                    screenCapContext.renderParameters.activeGltfCameraNodeId = node.m_ID;

                    core::logInfo("Camera selected: %d %s", glTFCameraID, node.getCamera()->getName().c_str());
                    gltfCamera = node.getCamera();
                }
            }

            if (gltfCamera == nullptr) {
                core::logError("Camera ID not found: %d", configuration.gltfCameraIndex.value());
                configuration.callbackError(false);
                return;
            }
        }
    }

    int height = configuration.height;
    int width = configuration.width;

    // Calculate image dimensions if only either width or height is given
    float cameraAspectRatio = screenCapContext.getRenderState().gltfLayer->getActiveCamera()->getAspectRatio();
    if (height <= 0 && cameraAspectRatio > 0) {
        height = width / cameraAspectRatio;
    }
    if (width <= 0 && cameraAspectRatio > 0) {
        width = height * cameraAspectRatio;
    }

    if (gltfCamera == nullptr) {
        camera.setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }

    // if the glTFCamera is used and has an aspect ratio then this needs to be used to calculate the resolution of the actual capture
    if (gltfCamera != nullptr && gltfCamera->getCameraPerspective().has_value() && gltfCamera->getCameraPerspective()->getAspectRatio().has_value()) {
        const double gltfCameraAspectRatio = gltfCamera->getCameraPerspective()->getAspectRatio().value();
        if (gltfCameraAspectRatio > 1) {
            height = width / gltfCameraAspectRatio;
        } else {
            width = height * gltfCameraAspectRatio;
        }
    }

    screenCapContext.setDimensions(width, height);
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    screenCapContext.getRenderState().gltfLayer->viewportAspectRatio = aspectRatio;

    // We need to explicitly set the aspect ratio for the glTFCamera if it is not defined (the mapping is called too late)
    if (gltfCamera != nullptr && gltfCamera->getCameraPerspective().has_value() && !gltfCamera->getCameraPerspective()->getAspectRatio().has_value()) {
        screenCapContext.getRenderState().gltfLayer->getActiveCamera()->setAspectRatio(aspectRatio);
    }

    int32_t numberOfFrames = 1;
    if (configuration.numberOfFrames.has_value()) {
        // Render the number of frames specified by the user
        numberOfFrames = configuration.numberOfFrames.value();
    } else if (configuration.animationEnabled) {
        // Render the number of frames needed to cover the whole animation
        float maxAnimationClipLength = screenCapContext.getRenderState().gltfLayer->getAnimationDuration();
        numberOfFrames = static_cast<int32_t>(math::ceil(maxAnimationClipLength * static_cast<float>(configuration.framerate)));
    }

    // make sure to render at least one frame
    if (numberOfFrames <= 0) {
        numberOfFrames = 1;
    }

    core::logInfo("Scheduled frames: %d", numberOfFrames);

    for (int32_t frameIndex = configuration.startFrameIndex.value_or(0); frameIndex < configuration.startFrameIndex.value_or(0) + numberOfFrames; frameIndex++) {
        // set animation
        const float animationTime = static_cast<float>(frameIndex) / static_cast<float>(configuration.framerate);
        if (configuration.animationEnabled) {
            screenCapContext.renderParameters.animationTime = animationTime;
        } else {
            screenCapContext.renderParameters.animationTime.reset();
        }
        screenCapContext.getRenderState().gltfLayer->applyAnimationTime(screenCapContext.renderParameters.animationTime);

        // render and store as image
        screenCapContext.paintGL(0.0f);
        QImage premultipliedAlpha(screenCapContext.getRenderState().framebuffer->toImage());
        QImage image(premultipliedAlpha.constBits(), premultipliedAlpha.width(), premultipliedAlpha.height(), QImage::Format_ARGB32);
        if (!configuration.alphaChannel) {
            image = image.convertToFormat(QImage::Format_RGB32);
        }

        // construct a filename and extract the output image format
        filesystem::Path path = filesystem::Path(QtFileSystem::instance(), configuration.outputFile);

        // Create path if it does not exist yet
        if (path.getName().find(".") <= path.getName().size()) {
            filesystem::Path dir = path.getParent();
            QDir qdir("");
            qdir.mkpath(dir.toString().c_str());
        }

        // if the path is only a single file but we render multiple we need to split it to add
        // a frameindex between file stem and extension
        core::string stem = path.toString();
        core::string extension = ".png";
        core::string fileFormat = "png";
        // get stem, extension and fileformat from path
        if (path.getName().find(".") <= path.getName().size()) {
            size_t lastDot = path.toString().rfind(".");
            stem = path.toString().substr(0, lastDot);
            extension = filesystem::File(path).getExtension();
            fileFormat = extension.substr(1);
        } else if (numberOfFrames == 1) {
            // if its a single file without extension, just add it to get a valid file
            path = filesystem::Path(QtFileSystem::instance(), stem + extension);
        }

        // append up to 3 zeros to the filename if necessary, i.e. <name>0003.png or <name>31416.png
        if (numberOfFrames > 1) {
            std::string frameCountText = std::to_string(frameIndex + 1);
            for (; frameCountText.length() < 4;) {
                frameCountText.insert(frameCountText.begin(), '0');
            }
            path = filesystem::Path(QtFileSystem::instance(), stem + frameCountText + extension);
        }

        // save rendered image
        if (false == image.save(path.toString().c_str(), fileFormat.c_str(), configuration.renderQuality)) {
            core::logInfo("Could not save image %s", path.toString().c_str());
            configuration.callbackError(false);
            return;
        }
        core::logInfo("Saved render capture at %s", path.toString().c_str());
    }

    // reset the current rendering by using the orignal camera
    screenCapContext.getRenderState().gltfLayer->setActiveCamera(originalCamera);

    // trigger an update on the original camera to recover after the aspect ratio might have been changed
    if (RendererContext* context = m_rendererContext.get(); context != nullptr) {
        context->dirtyStateChanged();
    }

    configuration.callbackSuccess(renderQueueIsBusy);
}

void GLTFRendererWidget::tabletEvent(QTabletEvent* event)
{
    if (event->type() == QEvent::TabletPress) {
        m_input.setUseTableMode(true);
        m_input.setLastTabletPos(event->globalPosition().toPoint());
    } else if (event->type() == QEvent::TabletRelease) {
        m_input.setUseTableMode(false);
    } else if (event->type() == QEvent::TabletMove) {
        m_input.setMouseDelta(event->globalPosition().toPoint());
    }
}

void GLTFRendererWidget::playAudio()
{
    if (m_rendererContext.get()->m_renderState.audioRenderer == nullptr) {
        return;
    }
    m_rendererContext.get()->m_renderState.audioRenderer->play();
}

void GLTFRendererWidget::stopAudio()
{
    if (m_rendererContext.get()->m_renderState.audioRenderer == nullptr) {
        return;
    }
    m_rendererContext.get()->m_renderState.audioRenderer->stop();
}

void GLTFRendererWidget::rewindAudio()
{
    if (m_rendererContext.get()->m_renderState.audioRenderer == nullptr) {
        return;
    }
    m_rendererContext.get()->m_renderState.audioRenderer->rewind();
}

void GLTFRendererWidget::pauseAudio()
{
    if (m_rendererContext.get()->m_renderState.audioRenderer == nullptr) {
        return;
    }
    m_rendererContext.get()->m_renderState.audioRenderer->pause();
}

void GLTFRendererWidget::toggleMute(bool unmute)
{
    if (m_rendererContext.get()->m_renderState.audioRenderer == nullptr) {
        return;
    }
    m_rendererContext.get()->m_renderState.audioRenderer->toggleMute(unmute);
}

void GLTFRendererWidget::setAudioPlaybackTime(float t)
{
    if (m_rendererContext.get()->m_renderState.audioRenderer == nullptr) {
        return;
    }
    m_rendererContext.get()->m_renderState.audioRenderer->setPlayBackTime(t);
}
