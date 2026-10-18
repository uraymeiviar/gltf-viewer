// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝
#include "Actions.h"
#include "PathConstants.h"
#include "core/Platform.h"
#include "interactors/GltfOperations.h"

#include "helpers/QtFileSystem.h"
#include "interactors/LoadInteractor.h"
#include "model/ProjectManager.h"
#include "useCases/LoadUseCases.h"
#include "widgets/MainWindow.h"
#include "widgets/QMLCard.h"

#include "pbr2gltf2.hpp"
#include <fmt/format.h>
#include <fstream>

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QKeySequence>

using namespace ux3d;
using namespace ux3d::editor;

Actions& Actions::instance()
{
    static Actions instance;
    return instance;
}

void Actions::setToolbarShortcutsEnabled(bool enabled)
{
    if (enabled) {
        toolSelect->setShortcutContext(Qt::ShortcutContext::ApplicationShortcut);
        toolMove->setShortcutContext(Qt::ShortcutContext::ApplicationShortcut);
        toolRotate->setShortcutContext(Qt::ShortcutContext::ApplicationShortcut);
        toolScale->setShortcutContext(Qt::ShortcutContext::ApplicationShortcut);
    } else {
        toolSelect->setShortcutContext(Qt::ShortcutContext::WidgetShortcut);
        toolMove->setShortcutContext(Qt::ShortcutContext::WidgetShortcut);
        toolRotate->setShortcutContext(Qt::ShortcutContext::WidgetShortcut);
        toolScale->setShortcutContext(Qt::ShortcutContext::WidgetShortcut);
    }
}

void Actions::updateAnimationControlsState()
{
    bool enableControls = model::gltf_operations::hasAnimations() || model::gltf_operations::hasEmitters();
    setAnimationControlsEnabled(enableControls);
}

void Actions::setAnimationControlsEnabled(bool enabled)
{
    selectAnimation->setEnabled(enabled);
    playingAnimation->setEnabled(enabled);
    stopAnimation->setEnabled(enabled);
    rewindAnimation->setEnabled(enabled);
    forwardAnimation->setEnabled(enabled);
}

void Actions::setRenderModeActionsEnabled(bool enabled)
{
    visualizationModeDefaultShaded->setEnabled(enabled);
    visualizationModeWireframe->setEnabled(enabled);
    for (QAction* action : visualizationModeDebugShadings) {
        action->setEnabled(enabled);
    }
}

void Actions::updateAnimationPopupState()
{
    selectAnimation->setEnabled(model::gltf_operations::hasAnimations()
        || model::gltf_operations::hasMorphing()
        || model::gltf_operations::hasSkinning());
}

void Actions::updateAudioMuteState()
{
    muteAudio->setEnabled(model::gltf_operations::hasEmitters());
}

Actions::Actions()
{
    newFile = new QAction("New Empty glTF", this);
    newFile->setShortcut(QKeySequence::New);

    newSampleFile = new QAction("New Sample glTF", this);

    open = new QAction("Open …", this);
    open->setShortcut(QKeySequence::Open);

    openRecent = new QAction("Open Recent", this);

    save = new QAction("Save", this);
    save->setShortcut(QKeySequence::Save);

    saveAs = new QAction("Save As …", this);
    saveAs->setShortcut(QKeySequence::SaveAs);

    saveAsMinified = new QAction("Save Minified As …", this);

    importImage = new QAction("Import Image …", this);
    importImage->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_I);

    importTexture = new QAction("Import Texture …", this);
    importTexture->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_T);

    importMaterial = new QAction("Import Material …", this);
    importMaterial->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_M);

    import3DModel = new QAction("Import 3D Model …", this);
    import3DModel->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_O);

    importXmpPacket = new QAction("Import XMP Packet …", this);

    importAudioSource = new QAction("Import Audio Source …", this);

    exportUSDZ = new QAction("Export USDZ …", this);

    quit = new QAction("Exit", this);
    quit->setShortcut(QKeySequence::Quit);
    quit->setMenuRole(QAction::QuitRole);

    undo = new QAction("Undo", this);
    undo->setShortcut(QKeySequence::Undo);

    redo = new QAction("Redo", this);
    redo->setShortcut(QKeySequence::Redo);

    copy = new QAction("Copy", this);
    copy->setShortcut(QKeySequence::Copy);
    copy->setEnabled(false);

    pasteLinked = new QAction("Paste Linked", this);
    pasteLinked->setShortcut(QKeySequence::Paste);
    pasteLinked->setEnabled(false);

    pasteDeep = new QAction("Paste Deep", this);
    pasteDeep->setEnabled(false);

    cut = new QAction("Cut", this);
    cut->setShortcut(QKeySequence::Cut);
    cut->setEnabled(true);

    remove = new QAction("Delete", this);
#ifdef UX3D_IS_APPLE
    remove->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_Backspace), QKeySequence::Delete });
#else
    remove->setShortcut(QKeySequence::Delete);
#endif
    remove->setEnabled(true);

    preferences = new QAction("Preferences …", this);
    preferences->setShortcut(QKeySequence::Preferences);
    preferences->setMenuRole(QAction::PreferencesRole);

    createScene = new QAction("Scene", this);
    createScene->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_S);

    createNode = new QAction("Node", this);
    createNode->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_N);

    createCamera = new QAction("Camera", this);
    createCamera->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_C);

    createLight = new QAction("Light", this);
    createLight->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_L);

    createMesh = new QAction("Mesh …", this);
    createMesh->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_E);

    createMaterial = new QAction("Material", this);
    createMaterial->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_M);

    createMaterialVariant = new QAction("Material Variant", this);
    createMaterialVariant->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_V);

    createTexture = new QAction("Texture", this);
    createTexture->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_T);

    createSampler = new QAction("Sampler", this);
    createSampler->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_A);

    createAnimation = new QAction("Animation …", this);
    createAnimation->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_K);

    createAudioEmitter = new QAction("Audio Emitter", this);

    createCameraFromView = new QAction("Add Camera From View", this);
    createCameraFromView->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_U);

    viewportMode = new QActionGroup(this);

    editMode = new QAction("Edit Mode", viewportMode);
    editMode->setShortcut({ Qt::CTRL | Qt::Key_F1 });
    editMode->setIconText("Edit Mode");
    editMode->setCheckable(true);
    editMode->setChecked(true);

    renderMode = new QAction("Render Mode", viewportMode);
    renderMode->setShortcut({ Qt::CTRL | Qt::Key_F2 });
    renderMode->setIconText("Render Mode");
    renderMode->setCheckable(true);

    tool = new QActionGroup(this);

    toolSelect = new QAction("Select", tool);
    toolSelect->setCheckable(true);
    toolSelect->setChecked(true);
    toolSelect->setShortcut(Qt::Key_Q);
    toolSelect->setIcon(QIcon(pathConstants::iconToolsSelect));
    toolSelect->setIconVisibleInMenu(false);

    toolMove = new QAction("Move", tool);
    toolMove->setCheckable(true);
    toolMove->setShortcut(Qt::Key_W);
    toolMove->setIcon(QIcon(pathConstants::iconToolsMove));
    toolMove->setIconVisibleInMenu(false);

    toolRotate = new QAction("Rotate", tool);
    toolRotate->setCheckable(true);
    toolRotate->setShortcut(Qt::Key_E);
    toolRotate->setIcon(QIcon(pathConstants::iconToolsRotate));
    toolRotate->setIconVisibleInMenu(false);

    toolScale = new QAction("Scale", tool);
    toolScale->setCheckable(true);
    toolScale->setShortcut(Qt::Key_R);
    toolScale->setIcon(QIcon(pathConstants::iconToolsScale));
    toolScale->setIconVisibleInMenu(false);

    editSpace = new QActionGroup(this);

    editSpaceWorld = new QAction("World Space", editSpace);
    editSpaceWorld->setShortcut({ Qt::CTRL | Qt::SHIFT | Qt::Key_G });
    editSpaceWorld->setCheckable(true);
    editSpaceWorld->setChecked(true);

    editSpaceLocal = new QAction("Local Space", editSpace);
    editSpaceLocal->setShortcut({ Qt::CTRL | Qt::SHIFT | Qt::Key_L });
    editSpaceLocal->setCheckable(true);

    editSpaceToggle = new QAction("Toggle Edit Space", editSpace);

    showGrid = new QAction("Show Grid", this);
    showGrid->setCheckable(true);
    showGrid->setChecked(true);

    showBoundingBoxes = new QAction("Show Bounding Boxes", this);
    showBoundingBoxes->setCheckable(true);
    showBoundingBoxes->setChecked(false);

    showSkinsAndBones = new QAction("Show Skins and Bones", this);
    showSkinsAndBones->setCheckable(true);
    showSkinsAndBones->setChecked(false);

    visualizationMode = new QActionGroup(this);

    visualizationModeDefaultShaded = new QAction("Default Shaded", visualizationMode);
    visualizationModeDefaultShaded->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_1);
    visualizationModeDefaultShaded->setCheckable(true);
    visualizationModeDefaultShaded->setChecked(true);

    visualizationModeWireframe = new QAction("Wireframe", visualizationMode);
    visualizationModeWireframe->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_2);
    visualizationModeWireframe->setCheckable(true);

    for (auto mode : m_debugOutputMapping) {
        auto* action = new QAction(QString::fromStdString(std::get<0>(mode)));
        action->setData((uint32_t)std::get<1>(mode));
        if (std::get<0>(mode).empty()) {
            action->setSeparator(true);
        }
        visualizationModeDebugShadings.push_back(action);
    }

    for (auto action : visualizationModeDebugShadings) {
        action->setCheckable(true);
        visualizationMode->addAction(action);
    }

    focus = new QAction("Focus Selected Node", this);
    focus->setShortcut(Qt::Key_F);

    muteAudio = new QAction("Mute Audio", this);
    QIcon muteAudioIcon;
    muteAudioIcon.addPixmap(QPixmap(pathConstants::iconAudioOn), QIcon::Mode::Normal, QIcon::State::On);
    muteAudioIcon.addPixmap(QPixmap(pathConstants::iconAudioOff), QIcon::Mode::Normal, QIcon::State::Off);
    muteAudioIcon.addPixmap(QPixmap(pathConstants::iconAudioDisabled), QIcon::Mode::Disabled, QIcon::State::On);
    muteAudioIcon.addPixmap(QPixmap(pathConstants::iconAudioDisabled), QIcon::Mode::Disabled, QIcon::State::Off);
    muteAudio->setIcon(muteAudioIcon);
    muteAudio->setCheckable(true);
    muteAudio->setChecked(true);

    selectAnimation = new QAction("Select Animations", this);
    selectAnimation->setIcon(QIcon(pathConstants::iconAnimationSelection));

    playingAnimation = new QAction("Play/Pause Animation", this);
    playingAnimation->setShortcuts({ Qt::Key_Space, Qt::Key_MediaTogglePlayPause });
    playingAnimation->setIcon(QIcon(pathConstants::iconTimelinePause));
    playingAnimation->setIconVisibleInMenu(false);

    stopAnimation = new QAction("Stop Animation", this);
    stopAnimation->setShortcuts({ Qt::Key_K, Qt::Key_MediaStop });
    stopAnimation->setIcon(QIcon(pathConstants::iconTimelineStop));
    stopAnimation->setIconVisibleInMenu(false);

    rewindAnimation = new QAction("Reset Animation", this);
    rewindAnimation->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
    rewindAnimation->setIcon(QIcon(pathConstants::iconTimelineResetBack));
    rewindAnimation->setIconVisibleInMenu(false);

    forwardAnimation = new QAction("Set Animation to End", this);
    forwardAnimation->setShortcuts({ { Qt::CTRL, Qt::Key_BracketRight }, Qt::Key_MediaLast });
    forwardAnimation->setIcon(QIcon(pathConstants::iconTimelineResetForward));
    forwardAnimation->setIconVisibleInMenu(false);

    projection = new QActionGroup(this);

    projectionPerspective = new QAction("Perspective", projection);
    projectionPerspective->setCheckable(true);
    projectionPerspective->setChecked(true);

    projectionOrthographic = new QAction("Orthographic", projection);
    projectionOrthographic->setCheckable(true);

    cameraOrientationTop = new QAction("Top", this);
    cameraOrientationTop->setShortcut(Qt::Key_1);
    cameraOrientationBottom = new QAction("Bottom", this);
    cameraOrientationBottom->setShortcut(Qt::Key_2);
    cameraOrientationRight = new QAction("Right", this);
    cameraOrientationRight->setShortcut(Qt::Key_3);
    cameraOrientationLeft = new QAction("Left", this);
    cameraOrientationLeft->setShortcut(Qt::Key_4);
    cameraOrientationFront = new QAction("Front", this);
    cameraOrientationFront->setShortcut(Qt::Key_5);
    cameraOrientationBack = new QAction("Back", this);
    cameraOrientationBack->setShortcut(Qt::Key_6);

    viewportResolution = new QAction("Resolution …", this);

    renderCapture = new QAction("Render Capture", this);
    renderCapture->setShortcut(Qt::Key_F5);

    removeMeshQuantizationFromGlTF = new QAction("Remove Mesh Quantization", this);
    removeDracoFromGlTF = new QAction("Remove Draco Mesh Compression", this);

    applyDracoToGlTF = new QAction("Apply Draco Mesh Compression", this);
    cleanExtensionsUsed = new QAction("Clean Used Extensions", this);
    cleanExtensionsUsed->setToolTip("Cleans the \"ExtensionsUsed\" and \"ExtensionsRequired\" properties of the glTF file by removing all entries from them not belonging to any used extension.");

    removeSpecular
        = new QAction("KHR_materials_specular", this);
    removeClearcoat = new QAction("KHR_materials_clearcoat", this);
    removeSheen = new QAction("KHR_materials_sheen", this);
    removeTransmission = new QAction("KHR_materials_transmission/KHR_materials_volume", this);
    removeVolume = new QAction("KHR_materials_volume", this);
    removeIridescence = new QAction("KHR_materials_iridescence", this);
    removeEmissiveStrength = new QAction("KHR_materials_emissive_strength", this);
    removeIOR = new QAction("KHR_materials_ior", this);
    removeSpecularGlossiness = new QAction("KHR_materials_pbrSpecularGlossiness", this);
    removeUnlit = new QAction("KHR_materials_unlit", this);
    removeTextureTransform = new QAction("KHR_texture_transform", this);
    removeLights = new QAction("KHR_lights_punctual", this);
    removeMaterialVariants = new QAction("KHR_materials_variants", this);
    removeUnknownExtensions = new QAction("Unknown", this);

    removeExtras = new QAction("Extras", this);
    removeNames = new QAction("Names", this);

    removeUnusedTexcoordAttributes = new QAction("Texture Coordinate Attributes", this);
    removeUnusedNodes = new QAction("Nodes", this);

    cleanup = new QAction("Cleanup All", this);
    cleanup->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_C);

    showInspector = new QAction("Show Inspector", this);
    showInspector->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    showInspector->setCheckable(true);
    showInspector->setChecked(true);

    showHierarchy = new QAction("Show Hierarchy", this);
    showHierarchy->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    showHierarchy->setCheckable(true);
    showHierarchy->setChecked(true);

    showViewport = new QAction("Show Viewport", this);
    showViewport->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
    showViewport->setCheckable(true);
    showViewport->setChecked(true);

    showResources = new QAction("Show Resources", this);
    showResources->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_4));
    showResources->setCheckable(true);
    showResources->setChecked(true);

    showLogger = new QAction("Show Logger", this);
    showLogger->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_5));
    showLogger->setCheckable(true);
    showLogger->setChecked(true);

    showViewportSettings = new QAction("Show Viewport Settings", this);
    showViewportSettings->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_6));
    showViewportSettings->setCheckable(true);
    showViewportSettings->setChecked(true);

    showStatistics = new QAction("Show Statistics", this);
    showStatistics->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_7));
    showStatistics->setCheckable(true);
    showStatistics->setChecked(true);

    resetLayout = new QAction("Reset Layout", this);
    resetLayout->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));

    about = new QAction("About", this);
    about->setMenuRole(QAction::AboutRole);
    showThirdPartySoftware = new QAction("Third-Party Software", this);
    showMITLicense = new QAction("Open Source License Notice", this);
    showPrivacyPolicy = new QAction("Privacy Policy", this);
    openDocumentation = new QAction("Documentation …", this);

    reloadStyleSheet = new QAction("Reload Stylesheet", this);
    reloadStyleSheetFromDisk = new QAction("Reload Stylesheet From Disk");

    // Content specific actions
    rename = new QAction("Rename", this);
    rename->setShortcut(Qt::Key_F2);
    setDefaultScene = new QAction("Set Selected as Default Scene", this);
    unsetDefaultScene = new QAction("Unset Default Scene", this);
    openScene = new QAction("Open Selected Scene", this);
    editImageExternally = new QAction("Edit in External Tool", this);

    setupConnections();
}

void Actions::setupConnections()
{
    connect(createNode, &QAction::triggered, this, [this] { createNodeFunction(); });
    connect(createScene, &QAction::triggered, this, [this] { createSceneFunction(); });
    connect(createCamera, &QAction::triggered, this, [this] { createCameraFunction(); });
    connect(createLight, &QAction::triggered, this, [this] { createLightFunction(); });
    connect(createMaterial, &QAction::triggered, this, [this] { createMaterialFunction(); });
    connect(createAnimation, &QAction::triggered, this, [this] { createAnimationFunction(); });
    connect(createTexture, &QAction::triggered, this, [this] { createTextureFunction(); });
    connect(createSampler, &QAction::triggered, this, [this] { createSamplerFunction(); });
    connect(createMesh, &QAction::triggered, this, [this] { createMeshFunction(); });
    connect(createMaterialVariant, &QAction::triggered, this, [this] { createMaterialVariantFunction(); });

    connect(importImage, &QAction::triggered, this, [this] { importImageFunction(); });
    connect(importTexture, &QAction::triggered, this, [this] { importImageAsTexture(); });
    connect(import3DModel, &QAction::triggered, this, [this] { import3DModelFunction(); });
    connect(importMaterial, &QAction::triggered, this, [this] { importMaterialFromPbr(); });
    connect(importXmpPacket, &QAction::triggered, this, [this] { importXmpPacketFunction(); });

    connect(removeMeshQuantizationFromGlTF, &QAction::triggered, this, [this] {
        auto taskDescription = nlohmann::json({ { "resolve_mesh_quantization_for_each", nlohmann::json::object() } });
        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
    });

    connect(removeDracoFromGlTF, &QAction::triggered, this, [this] {
        auto taskDescription = nlohmann::json({ { "resolve_draco_for_each", nlohmann::json::object() } });
        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
    });

    connect(applyDracoToGlTF, &QAction::triggered, this, [this] {
        auto mainWindow = MainWindow::getMainWindow();
        if (mainWindow) {
            mainWindow->showPopup("qrc:/Popups/DracoAll.qml", "Draco Mesh Compression", 512, 380);
        }
    });

    connect(cleanExtensionsUsed, &QAction::triggered, this, [this] {
        auto taskDescription = nlohmann::json({ { "update_extension_flags", nlohmann::json::object() } });
        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription), "Cleaned used extensions.");
    });

    connect(removeClearcoat, &QAction::triggered, this, [this] { removeExtension(removeClearcoat->text()); });
    connect(removeSpecular, &QAction::triggered, this, [this] { removeExtension(removeSpecular->text()); });
    connect(removeSheen, &QAction::triggered, this, [this] { removeExtension(removeSheen->text()); });
    connect(removeTransmission, &QAction::triggered, this, [this] {
        const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
        auto taskDescription = nlohmann::json({ { "remove_extension", { { "properties", { "KHR_materials_transmission", "KHR_materials_volume" } } } } });
        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
    });
    connect(removeVolume, &QAction::triggered, this, [this] { removeExtension(removeVolume->text()); });
    connect(removeIridescence, &QAction::triggered, this, [this] { removeExtension(removeIridescence->text()); });
    connect(removeEmissiveStrength, &QAction::triggered, this, [this] { removeExtension(removeEmissiveStrength->text()); });
    connect(removeIOR, &QAction::triggered, this, [this] { removeExtension(removeIOR->text()); });
    connect(removeSpecularGlossiness, &QAction::triggered, this, [this] { removeExtension(removeSpecularGlossiness->text()); });
    connect(removeUnlit, &QAction::triggered, this, [this] { removeExtension(removeUnlit->text()); });
    connect(removeTextureTransform, &QAction::triggered, this, [this] { removeExtension(removeTextureTransform->text()); });
    connect(removeLights, &QAction::triggered, this, [this] { removeExtension(removeLights->text()); });
    connect(removeMaterialVariants, &QAction::triggered, this, [this] { removeExtension(removeMaterialVariants->text()); });
    connect(removeExtras, &QAction::triggered, this, [this] {
        const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
        auto taskDescription = nlohmann::json({ { "remove_properties_by_name", { { "property", "/extras" } } } });
        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription), "", true);
    });
    connect(removeNames, &QAction::triggered, this, [this] {
        const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
        auto taskDescription = nlohmann::json({ { "remove_properties_by_name", { { "property", "/name" } } } });
        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription), "", true);
    });
}

void Actions::removeExtension(const QString& extension)
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    auto taskDescription = nlohmann::json({ { "remove_extension", nlohmann::json::object() } });
    taskDescription["remove_extension"]["property"] = extension.toStdString();
    QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
}

void Actions::createNodeFunction()
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    auto taskDescription = nlohmann::json({ { "create_gltf_object", nlohmann::json::object() } });
    taskDescription["create_gltf_component"]["path"] = "/gltf/nodes";
    QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
}

void Actions::createSceneFunction()
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    auto taskDescription = nlohmann::json({ { "create_gltf_object", nlohmann::json::object() } });
    taskDescription["create_gltf_component"]["path"] = "/gltf/scenes";
    QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
}

void Actions::createCameraFunction()
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    auto taskDescription = nlohmann::json({ { "create_gltf_component", nlohmann::json::object() } });
    taskDescription["create_gltf_component"]["path"] = "/gltf/cameras";
    taskDescription["create_gltf_component"]["type"] = "perspective";
    taskDescription["create_gltf_component"]["perspective"] = nlohmann::json::object();
    taskDescription["create_gltf_component"]["perspective"]["znear"] = 0.1f;
    taskDescription["create_gltf_component"]["perspective"]["zfar"] = 10;
    taskDescription["create_gltf_component"]["perspective"]["yfov"] = 0.6605925559997559;
    QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
}

void Actions::createLightFunction()
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    auto taskDescription = nlohmann::json({ { "create_gltf_component", nlohmann::json::object() } });
    taskDescription["create_gltf_component"]["path"] = "/gltf/extensions/KHR_lights_punctual/lights";
    taskDescription["create_gltf_component"]["type"] = "spot";
    QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
}

void Actions::createMaterialFunction()
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    auto taskDescription = nlohmann::json({ { "create_gltf_object", nlohmann::json::object() } });
    taskDescription["create_gltf_component"]["path"] = "/gltf/materials";
    QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
}

void Actions::createAnimationFunction()
{
    auto mainWindow = MainWindow::getMainWindow();
    if (mainWindow) {
        mainWindow->showPopup("qrc:/Popups/CreateAnimation.qml", "Create Animation", 310, 380);
    }
}

void Actions::createTextureFunction()
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    auto taskDescription = nlohmann::json({ { "create_gltf_object", nlohmann::json::object() } });
    taskDescription["create_gltf_component"]["path"] = "/gltf/textures";
    QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
}

void Actions::createSamplerFunction()
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    auto taskDescription = nlohmann::json({ { "create_gltf_object", nlohmann::json::object() } });
    taskDescription["create_gltf_component"]["path"] = "/gltf/samplers";
    QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
}

void Actions::createMeshFunction()
{
    auto mainWindow = MainWindow::getMainWindow();
    if (mainWindow) {
        mainWindow->showPopup("qrc:/Popups/CreateMesh.qml", "Create Mesh", 200, 380);
    }
}

void Actions::createMaterialVariantFunction()
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    auto taskDescription = nlohmann::json({ { "create_gltf_object", nlohmann::json::object() } });
    taskDescription["create_gltf_component"]["path"] = "/gltf/extensions/KHR_materials_variants/variants";
    taskDescription["create_gltf_component"]["name"] = "Material variant";
    QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription));
}

void Actions::importImageFunction()
{
    showImportDialog("Images (*.png *.jpg *.jpeg *.webp)", "Import image", [this](const QString& filename) {
        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule({ {
            "import_image",
            {
                { "filepath", filename.toStdString() },
            },
        } }));
    });
}

void Actions::importImageAsTexture()
{
    showImportDialog("Images (*.png *.jpg *.jpeg)", "Import texture", [this](const QString& filename) {
        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule({ {
            "import_texture",
            {
                { "filepath", filename.toStdString() },
            },
        } }));
    });
}

void Actions::import3DModelFunction()
{
    // To support more file formats, assimp build has to be configured in Gestaltor/thirdparty/assimp.cmake accordingly
    showImportDialog("3D Models (*.gltf *.glb *.blend *.3ds *.dxf *.fbx *.gltf *.obj *.ply *.stl *.x3d *.gestalt)", "Import 3D model", [this](const QString& filename) {
        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule({ {
            "import_asset",
            {
                { "filepath", filename.toStdString() },
            },
        } }));
    });
}

void Actions::importMaterialFromPbr()
{
    QFileDialog* fileDialog = new QFileDialog(nullptr, "Select Directory", "");
    fileDialog->setLabelText(QFileDialog::Accept, tr("Select"));
    fileDialog->setFileMode(QFileDialog::Directory);

#ifdef UX3D_IS_APPLE
    fileDialog->setModal(false);
#else
    fileDialog->setModal(true); // force native dialog on windows
#endif

    connect(fileDialog, &QFileDialog::accepted, [this, fileDialog] {
        const QList<QUrl>& dirNames = fileDialog->selectedUrls();
        fileDialog->deleteLater();
        if (dirNames.empty()) {
            return;
        }
        for (auto dirName : dirNames) {
            filesystem::Path path(QtFileSystem::instance(), dirName.toLocalFile().toStdString());
            QtFileSystem::sandboxSaveBookmark(path);
        }
        QString dirName = dirNames.value(0).toLocalFile();

#ifdef UX3D_IS_APPLE
        filesystem::Path targetDirectory = QtFileSystem::instance().toPath(dirName.toStdString());
        filesystem::IFileSystem::sandboxRequestAccess(targetDirectory);
#endif

        // make sure the directory exists and is empty
        QDir temp = QDir::temp();
        temp.cd("pbr2gltf2");
        temp.removeRecursively();

        temp = QDir::temp();
        temp.mkdir("pbr2gltf2");
        temp.cd("pbr2gltf2");

        pbr2gltf2::convert_to_filesystem(dirName.toStdString(), temp.path().toStdString(), 1.0f, 1.0f, true, true);

        temp.setNameFilters(QStringList() << "*.gltf");
        QStringList fileList = temp.entryList();

        QString fileName = fileList.at(0);

        if (!dirName.isEmpty()) {
            auto taskDescription = nlohmann::json({ { "merge_gltf", nlohmann::json::object() } });
            taskDescription["merge_gltf"]["filepath"] = temp.path().toStdString() + "/" + fileName.toStdString();
            model::ProjectManager::getInstance()->model.schedule(taskDescription);
        }
    });

    fileDialog->show();
}

void Actions::importXmpPacketFunction()
{
    showImportDialog("JSON (*.json)", "Open XMP packet", [this](const QString& filename) {
        std::ifstream ifs(filename.toStdString());
        nlohmann::json packet = nlohmann::json::parse(ifs, nullptr, false);
        if (packet.is_discarded()) {
            QMLCard::error(QString::fromStdString(fmt::format("JSON file is invalid and could not be imported. ({})", filename.toStdString())));
            return;
        }
        nlohmann::json packetsArray = nlohmann::json::array();
        packetsArray.push_back(packet);
        ifs.close();
        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule({ {
            "create_gltf_component",
            {
                { "path", "/gltf/extensions/KHR_xmp_json_ld/packets" },
            },
        } }));
    });
}

void Actions::showImportDialog(const QString& filter, const QString& caption, std::function<void(const QString&)> func)
{
    QFileDialog* fileDialog = new QFileDialog(nullptr, caption, "", filter);
    fileDialog->setLabelText(QFileDialog::Accept, tr("Import"));
    fileDialog->setFileMode(QFileDialog::ExistingFiles);

#ifdef UX3D_IS_APPLE
    fileDialog->setModal(false);
#else
    fileDialog->setModal(true); // force native dialog on windows
#endif

    connect(fileDialog, &QFileDialog::accepted, [this, fileDialog, func] {
        const QStringList& filenames = fileDialog->selectedFiles();
        fileDialog->deleteLater();
        for (auto filename : filenames) {
            if (filename.isEmpty()) {
                continue;
            }
            filesystem::Path path(QtFileSystem::instance(), filename.toStdString());
            QtFileSystem::sandboxSaveBookmark(path);
#ifdef UX3D_IS_APPLE
            filesystem::Path targetDirectory = QtFileSystem::instance().toFile(filename.toStdString()).getParent();
            filesystem::IFileSystem::sandboxRequestAccess(targetDirectory);
#endif
            func(filename);
        }
    });

    fileDialog->show();
}