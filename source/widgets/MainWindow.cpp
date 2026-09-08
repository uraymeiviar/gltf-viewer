#include "widgets/MainWindow.h"
#include "Actions.h"
#include "Clipboard.h"
#include "Constants.h"
#include "ExternalEditingManager.h"
#include "HierarchyModel.h"
#include "HotReloadQQuickView.h"
#include "ImageProvider.h"
#include "InspectorModel.h"
#include "PathConstants.h"
#include "Preferences.h"
#include "RxRunLoop.h"
#include "SelectionManager.h"
#include "ValidationModel.h"
#include "Version.h"
#include "core/Platform.h"
#include "helpers/FileFilters.h"
#include "helpers/GltfCleanup.h"
#include "helpers/QtFileSystem.h"
#include "helpers/RecentFileHelper.h"
#include "helpers/Utils.h"
#include "interactors/GltfInteractor.h"
#include "interactors/GltfOperations.h"
#include "interactors/LoadInteractor.h"
#include "interactors/SaveInteractor.h"
#include "model/ProjectManager.h"
#include "operations/Library.h"
#include "reactor/Gltf.h"
#include "reactor/Schema.h"
#include "reactor/Validation.h"
#include "useCases/GltfUseCases.h"
#include "useCases/LoadUseCases.h"
#include "widgets/CustomDockWidget.h"
#include "widgets/CustomTitleWidget.h"
#include "widgets/Dialog.h"
#include "widgets/GLTFEditorWidget.h"
#include "widgets/Hierarchy.h"
#include "widgets/Inspector.h"
#include "widgets/Logger.h"
#include "widgets/QMLCard.h"
#include "widgets/QMLMessageBox.h"
#include "widgets/RenderView.h"
#include "widgets/StatisticsWidget.h"
#include "widgets/ViewportSettings.h"
#include "widgets/renderContext/RendererContext.h"

#include <filesystem>
#include <string>

#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QQuickItem>
#include <QSettings>
#include <QStandardPaths>
#include <QWindow>

using namespace ux3d;
using namespace ux3d::editor;

MainWindow::MainWindow(QWidget* _pParent, const core::string _logFilename)
    : QMainWindow(_pParent)
{
    m_instance = this;
    QQuickWindow::setDefaultAlphaBuffer(true);

    rxRunLoop = new RxRunLoop(this);

    auto convertSelectionToOldSystem = [=](const std::tuple<nlohmann::json, nlohmann::json>& tuple, gltf::Gltf* gltf) {
        const auto& [selection, uuidPaths] = tuple;
        nlohmann::json selectionCopy = selection;
        if (selectionCopy.empty()) {
            selectionCopy = SelectionManager::getInstance()->getCurrentJsonSelection();
        }
        if (blockSelectionHeadChanged) {
            return;
        }

        // select node
        std::vector<SelectionInfo> selectionInfos;
        if (selectionCopy.size() == 0) {
            SelectionInfo selectInfo;
            selectInfo.pointer = std::monostate();
            selectInfo.type = GltfPropertyType::None;
            selectInfo.source = SelectionSource::Content;
            selectionInfos.push_back(selectInfo);
        } else {
            for (const auto& selectionObj : selectionCopy) {
                SelectionInfo selectInfo;
                const std::string& uuid = selectionObj["elementUUID"];
                if (uuid.empty()) {
                    continue;
                }
                if (!uuidPaths.contains(uuid)) {
                    continue;
                }
                nlohmann::json::json_pointer jsonPointer;
                try {
                    jsonPointer = nlohmann::json::json_pointer(uuidPaths[uuid]);
                } catch (...) {
                    continue;
                }
                auto [gltfPointer, gltfType] = jsonPointerToGltfPointer(jsonPointer, gltf ? *gltf : model::ProjectManager::getInstance()->getGltf());
                selectInfo.pointer = gltfPointer;
                selectInfo.type = gltfType;

                // Content always creates selections on objects with a UUID, never references
                if (selectionObj["localPath"].get<std::string>().empty()) {
                    // It is important to set this to Content for selections that originate
                    // from content as we otherwise get stuck in an infinite loop (see Content::receiveSelection)
                    selectInfo.source = SelectionSource::Content;
                } else {
                    selectInfo.source = SelectionSource::Hierarchy;
                }
                selectionInfos.push_back(selectInfo);
            }
        }

        blockSelectionSignalUpdate = true;
        SelectionManager::getInstance()->selectObjects(selectionInfos);
        blockSelectionSignalUpdate = false;
    };

    model::ProjectManager::init(rxRunLoop->worker(), rxRunLoop->worker(), convertSelectionToOldSystem);

    auto onError = [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        }
    };

    model::ProjectManager::getInstance()->model.canUndo.observe_on(rxRunLoop->worker()).subscribe([](bool canUndo) { Actions::instance().undo->setEnabled(canUndo); }, onError);

    model::ProjectManager::getInstance()->model.canRedo.observe_on(rxRunLoop->worker()).subscribe([](bool canRedo) { Actions::instance().redo->setEnabled(canRedo); }, onError);

    model::ProjectManager::getInstance()->dirty.subscribe([this](bool dirty) {
        m_dirty = dirty;
        setWindowModified(dirty);
    },
        onError);

    m_applicationData = new ApplicationData(this);

    m_qmlEngine = new QQmlEngine(this);
    auto path = UX3D_QML_ROOT_DIR;
    m_qmlEngine->addImportPath(QString(path));
    m_qmlEngine->addImportPath(QString(path) + "/imports");
    m_qmlEngine->addImportPath(QString(path) + "/js");
    m_qmlEngine->addImportPath(QString(path) + "/Model");
    m_qmlEngine->addImportPath(QString(path) + "/Components");

    m_qmlEngine->rootContext()->setContextProperty("applicationData", static_cast<QObject*>(m_applicationData));
    m_qmlEngine->addImageProvider(QLatin1String("provider"), new editor::ImageProvider());
    qmlRegisterType<HierarchyModel>("HierarchyModel", 1, 0, "HierarchyModel");
    qmlRegisterType<InspectorModel>("InspectorModel", 1, 0, "InspectorModel");
    qmlRegisterType<ValidationModel>("ValidationModel", 1, 0, "ValidationModel");

    setupUi(this);

    GltfInteractor::getInstance()->registerOutput(this);
    LoadInteractor::getInstance()->registerOutput(this);

    setWindowIcon(QIcon(":/resources/png/Assets/Icon_ApplicationWindow.png"));
    updateWindowTitle();

    setTabPosition(Qt::DockWidgetArea::AllDockWidgetAreas, QTabWidget::North);

    m_pHierarchyDock = initializeHierarchy();
    m_pInspectorDock = initializeInspector();
    m_pRendererDock = initializeRenderer();
    m_pResourcesDock = initializeResources();
    m_pLoggerDock = initializeLogger();
    initializePreferences();
    initializeAboutGestaltor();
    initializeThirdParties();
    initializeMITLicense();
    m_pViewportSettingsDock = initializeViewportSettings();
    m_pStatisticsDock = initializeStatistics();
    m_popupTemplate = new HotReloadQQuickView(m_qmlEngine);
    m_popupTemplate->setFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    m_popupTemplate->setModality(Qt::ApplicationModal);

    connect(static_cast<QApplication*>(QApplication::instance()), &QGuiApplication::applicationStateChanged, this, [this]() {
        changeAlwaysOnTop(m_popupTemplate);
    });

    m_validationAfterSave = new QMLMessageBox("glTF has validation issues", m_qmlEngine);

    connect(static_cast<QApplication*>(QApplication::instance()), &QGuiApplication::applicationStateChanged, this, [this]() {
        changeAlwaysOnTop(m_validationAfterSave);
    });

    core::logInfo("Logs are saved in '%s'", _logFilename.c_str());

    const QString applicationDataPath = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    QSettings userConfig(applicationDataPath + "/userConfig.ini", QSettings::IniFormat);
    m_recentFileHelper = new utils::RecentFilesHelper(userConfig);

    setAcceptDrops(true);

    setContextMenuPolicy(Qt::ContextMenuPolicy::NoContextMenu);

    setupMenuBarActions();

    // Required for dynamically created widgets in combination with Qt controlled ui elements
    // TODO: After refactoring and cleanup, can probably be removed.
    loadStyleSheet();

    initializeLayout();
    restoreSession();
    show();

    {
        auto id = std::this_thread::get_id();

        auto changedOnMainThread = model::ProjectManager::getInstance()->headChangeWithoutStateChanged.get_observable().filter([id](std::monostate) { return id == std::this_thread::get_id(); });

        auto changedOnBackgroundThread = model::ProjectManager::getInstance()->headChangeWithoutStateChanged.get_observable().filter([id](std::monostate) { return id != std::this_thread::get_id(); });

        changedOnMainThread
            .merge(changedOnBackgroundThread.observe_on(rxRunLoop->worker()))
            .subscribe([](std::monostate) {
                LoadInteractor::getInstance()->reloadGltf();
            },
                onError);
    }

    model::ProjectManager::getInstance()->editorModel.selection.observe_on(rxRunLoop->worker()).with_latest_from(model::ProjectManager::getInstance()->model.uuidPaths).subscribe([convertSelectionToOldSystem](const std::tuple<nlohmann::json, nlohmann::json>& tuple) { convertSelectionToOldSystem(tuple, nullptr); }, onError);

    model::ProjectManager::getInstance()->editorModel.wireframe.observe_on(rxRunLoop->worker()).with_latest_from(model::ProjectManager::getInstance()->model.uuidPaths).subscribe([=](const std::tuple<nlohmann::json, nlohmann::json>& tuple) {
        const auto& [patch, uuidPaths] = tuple;
        std::string patchPath = patch["path"];
        std::string uuid;
        bool wireframe = false;

        // if multiple meshes are in wireframe mode and you change the state of one of them,
        // json diff will generate multiple ops for this one operation
        // (replace the object in question with the next element, repeat until last element reached, remove last element).
        // However only the first operation in the diff has the previous state (see model/Model.cpp).
        // We can therefore handle the first replace operation (with the prevState) like a delete
        // and skip all following operations without the "prevState".

        // skip operation of no prevState present
        if (patch.contains("prevState") == false) {
            return;
        }

        if (patch["op"] == "add") {
            if (patch["value"].is_array()) {
                uuid = patch["value"][0];
            } else {
                uuid = patch["value"];
            }
        } else if (patch["op"] == "remove" || patch["op"] == "replace") {
            // remove and replace can be handled identical
            // retrieve deleted path from previous wireframe state
            nlohmann::json::json_pointer deletedJsonPointer = nlohmann::json::json_pointer(patchPath);
            uuid = patch["prevState"][deletedJsonPointer];
            wireframe = true;
        } else {
            core::debugLog(core::LogLevel::Warning, "An unforseen state occured when handling mesh wireframe mode update.");
            return;
        }

        // update mesh (for renderer update)
        // update mesh (for renderer update)
        nlohmann::json::json_pointer jsonPointer = nlohmann::json::json_pointer(uuidPaths[uuid]);
        auto [gltfPointer, gltfType] = jsonPointerToGltfPointer(jsonPointer,
            model::ProjectManager::getInstance()->getGltf());
        if (gltfType == GltfPropertyType::Meshes) {
            gltf::Mesh* pMesh = gltfPointer.get<gltf::Mesh*>();
            pMesh->setFilled(wireframe);
            model::ProjectManager::getInstance()->stateChangedTemporarily();
        }
    },
        onError);

    model::ProjectManager::getInstance()->editorModel.visibility.observe_on(rxRunLoop->worker()).with_latest_from(model::ProjectManager::getInstance()->model.uuidPaths).subscribe([=](const std::tuple<nlohmann::json, nlohmann::json>& tuple) {
        const auto& [patch, uuidPaths] = tuple;
        std::string patchPath = patch["path"];
        std::string uuid;
        bool visibility = false;

        // if multiple meshes are invisible and you change the state of one of them,
        // json diff will generate multiple ops for this one operation
        // (replace the object in question with the next element, repeat until last element reached, remove last element).
        // However only the first operation in the diff has the previous state (see model/Model.cpp).
        // We can therefore handle the first replace operation (with the prevState) like a delete
        // and skip all following operations without the "prevState".

        // skip operation of no prevState present
        if (patch.contains("prevState") == false) {
            return;
        }

        if (patch["op"] == "add") {
            if (patch["value"].is_array()) {
                uuid = patch["value"][0];
            } else {
                uuid = patch["value"];
            }
        } else if (patch["op"] == "remove" || patch["op"] == "replace") {
            // remove and replace can be handled identical
            // retrieve deleted path from previous wireframe state
            nlohmann::json::json_pointer deletedJsonPointer = nlohmann::json::json_pointer(patchPath);
            uuid = patch["prevState"][deletedJsonPointer];
            visibility = true;
        } else {
            core::debugLog(core::LogLevel::Warning, "An unforseen state occured when handling mesh visibility mode update.");
            return;
        }

        // update mesh (for renderer update)
        nlohmann::json::json_pointer jsonPointer = nlohmann::json::json_pointer(uuidPaths[uuid]);

        auto [gltfPointer, gltfType] = jsonPointerToGltfPointer(jsonPointer, model::ProjectManager::getInstance()->getGltf());
        if (gltfType == GltfPropertyType::Meshes) {
            gltf::Mesh* pMesh = gltfPointer.get<gltf::Mesh*>();
            pMesh->setVisible(visibility);
            model::ProjectManager::getInstance()->meshVisualizationChanged();
            model::ProjectManager::getInstance()->stateChangedTemporarily();
        }
        if (gltfType == GltfPropertyType::Nodes) {
            gltf::Node* pNode = gltfPointer.get<gltf::Node*>();
            pNode->setVisible(visibility);
            model::ProjectManager::getInstance()->nodeVisualizationChanged();
            model::ProjectManager::getInstance()->stateChangedTemporarily();
        }
    },
        onError);

    // The legacy selection manager requires a current glTF-object to UUID mapping.
    model::ProjectManager::getInstance()->model.uuids.observe_on(rxRunLoop->worker()).subscribe([this](const nlohmann::json& uuids) {
        this->uuids = uuids;

        // If a property was created by the legacy system, select it once the UUID is created
        if (!createdProperty.empty()) {
            nlohmann::json::json_pointer tmpPointer = ("/gltf"_json_pointer / createdProperty / "uuid");
            if (uuids.contains(tmpPointer)) {
                createdProperty = nlohmann::json::json_pointer();
                std::string uuid = uuids.at(tmpPointer);
                Clipboard::instance().clearAndSetSelection(QString::fromStdString(uuid), "");
            }
        }
    },
        onError);

    connect(SelectionManager::getInstance(), &SelectionManager::selectObjects, this,
        [this](const std::vector<SelectionInfo>& selections) {
            if (blockSelectionSignalUpdate) {
                return;
            }

            blockSelectionHeadChanged = true;
            if (selections.empty()) {
                model::ProjectManager::getInstance()->editorModel.run(nlohmann::json({ { "editor.clearSelection", nlohmann::json({}) } }));
                blockSelectionHeadChanged = false;
                return;
            }

            nlohmann::json selectionJSON = nlohmann::json::array();
            for (const auto& selection : selections) {
                // skip empty selections (mainly error correction)
                if (selection.type == GltfPropertyType::None) {
                    continue;
                }

                std::string uuid;
                std::string localPath = "";
                const gltf::Scene* scene = m_pRenderer->getCurrentScene();
                // If we get a selection from renderview we search for the parent scene/node to select it in the hierarchy
                if (selection.source == SelectionSource::Renderview && scene != nullptr && selection.type == GltfPropertyType::Nodes) {
                    auto nodeVector = scene->getNodes();
                    bool found = false;
                    // First check if it the node is a direct child of the open scene
                    for (int i = 0; i < nodeVector.size(); ++i) {
                        if (nodeVector[i] == selection.pointer.unsafeGet<gltf::Node*>()) {
                            localPath = "/nodes/" + std::to_string(i);
                            found = true;
                            uuid = uuids["/gltf"_json_pointer / gltfPointerToJsonPointer(scene, GltfPropertyType::Scenes) / "uuid"];
                            break;
                        }
                    }
                    // If not found check all nodes if the selected node is a child. This should always succeed.
                    if (!found) {
                        int counter = 0;
                        for (const gltf::Node& node : model::ProjectManager::getInstance()->getGltf().getNodes()) {
                            auto nodeVector2 = node.getChildren();
                            for (int i = 0; i < nodeVector2.size(); ++i) {
                                if (nodeVector2[i] == selection.pointer.unsafeGet<gltf::Node*>()) {
                                    localPath = "/children/" + std::to_string(i);
                                    found = true;
                                    uuid = uuids["/gltf/nodes"_json_pointer / counter / "uuid"];
                                    break;
                                }
                            }
                            ++counter;
                        }
                    }
                    if (!found) {
                        // This should never happen
                        uuid = uuids["/gltf"_json_pointer / gltfPointerToJsonPointer(toConst(selection.pointer), selection.type) / "uuid"];
                    }
                } else {
                    uuid = uuids["/gltf"_json_pointer / gltfPointerToJsonPointer(toConst(selection.pointer), selection.type) / "uuid"];
                }

                nlohmann::json selectionObj = {
                    { "uuid", uuid },
                    { "localPath", localPath }
                };
                selectionJSON.push_back(selectionObj);
            }

            if (selectionJSON.size() > 0) {
                model::ProjectManager::getInstance()->editorModel.run(nlohmann::json({ { "editor.setSelection", selectionJSON } }));
            } else {
                model::ProjectManager::getInstance()->editorModel.run(nlohmann::json({ { "editor.clearSelection", nlohmann::json({}) } }));
            }
            blockSelectionHeadChanged = false;
        });
    if (Preferences::getInstance()->getInitialFilename().isEmpty() && !model::ProjectManager::getInstance()->isCMDMode()) {
#ifndef UX3D_IS_APPLE
        if (Preferences::getInstance()->getLoadSampleAssetOnStartup()) {
            newSampleFile();
        }
#endif
    }

    environmentCompatibiliyCheck();

    // Initialize clipboard
    Clipboard::instance();

    model::ProjectManager::getInstance()->version = Version::get().getReleaseVersion();
}

void MainWindow::showPopup(const QString& file, const QString& title, int height, int width, int x, int y)
{
    if (m_popupTemplate) {
        m_popupTemplate->setTitle(title);
        m_popupTemplate->setSource(file);
        m_popupTemplate->setMaximumSize(QSize(width, height));
        m_popupTemplate->setMinimumSize(QSize(width, height));
        if (x == -1 || y == -1) {
            x = this->width() / 2 - width / 2;
            y = this->height() / 2 - height / 2;
        }
        m_popupTemplate->show();
        m_popupTemplate->resize(width, height);
        m_popupTemplate->setPosition(x, y);
    }
}

void MainWindow::closePopup()
{
    if (m_popupTemplate) {
        m_popupTemplate->close();
    }
}

void MainWindow::queueCapture(const CommandLineParsing::CaptureConfiguration& _configuration)
{
    if (!model::ProjectManager::getInstance()->isCMDMode()) {
        return;
    }

    // play all specified animation clips
    model::GltfBinding gltfBinding;
    gltfBinding.disableAllAnimations();
    if (_configuration.animationClips.empty() == false) {
        for (uint32_t index : _configuration.animationClips) {
            gltfBinding.setAnimationPlaying(index, true);
        }
    }
    if (gltfBinding.arePlayingAnimationsConflicting()) {
        core::logError("Animation clips are conflicting, please try different arguments.");
        emit exited();
        return;
    }

    editor::CaptureConfiguration parameters;

    if (_configuration.sceneIndex >= 0) {
        parameters.gltfSceneIndex = _configuration.sceneIndex;
    } else {
        // render default scene or first scene if none is provided
        int32_t defaultSceneIndex = GltfInteractor::getInstance()->getDefaultSceneIndex();
        if (defaultSceneIndex >= 0) {
            parameters.gltfSceneIndex = defaultSceneIndex;
        } else {
            parameters.gltfSceneIndex = 0;
        }
    }

    if (_configuration.toneMapping.has_value()) {
        parameters.toneMapping = static_cast<renderer::ToneMappingsEnum>(_configuration.toneMapping.value());
    }
    parameters.gltfCameraIndex = _configuration.cameraIndex;
    parameters.clearColor = _configuration.clearColor;
    parameters.alphaChannel = _configuration.alphaChannel;

    parameters.numberOfFrames = _configuration.numberOfFrames;
    parameters.outputFile = _configuration.outputPath.toStdString();
    parameters.width = _configuration.width;
    parameters.height = _configuration.height;
    parameters.environmentMapPath = filesystem::Path(QtFileSystem::instance(), _configuration.iblPath.toStdString());
    parameters.framerate = _configuration.framerate;
    parameters.animationEnabled = _configuration.animationEnabled;
    parameters.radius = _configuration.radius;
    parameters.theta = _configuration.theta;
    parameters.phi = _configuration.phi;
    parameters.target = _configuration.target;
    parameters.vFoV = _configuration.vFoV;
    parameters.callbackError = [&](bool _busy) {
        core::logError("Queued rendering failed");
        if (!_busy) {
            emit exited();
        }
    };
    parameters.callbackSuccess = [&](bool _busy) {
        if (!_busy) {
            emit exited();
        }
    };

    m_pRenderer->queueCapture(parameters);
}

MainWindow::~MainWindow()
{
    GltfInteractor::getInstance()->unregisterOutput(this);
    LoadInteractor::getInstance()->unregisterOutput(this);

    if (m_preferencesWindow != nullptr) {
        delete m_preferencesWindow;
        m_preferencesWindow = nullptr;
    }
}

void MainWindow::presentGltfOpen(gltf::Gltf* _pGltf)
{
    presentGltfReloaded(_pGltf);

    model::ProjectManager::getInstance()->editorModel.state.get_subscriber().on_next(nlohmann::json::object());
}

void MainWindow::presentGltfReloaded(gltf::Gltf*)
{
    gltf::Scene* pScene = GltfInteractor::getInstance()->getDefaultScene();
}

void MainWindow::presentGltfImport(gltf::Gltf*)
{
    gltf::Scene* pScene = GltfInteractor::getInstance()->getDefaultScene();
}

void MainWindow::presentSceneOpen(const GltfSceneOpenUseCase& _useCase)
{
    updateOpenSceneUI(_useCase.pScene);
}

void MainWindow::updateOpenSceneUI(const gltf::Scene* scene)
{
    std::string jsonPath = "";
    if (scene != nullptr) {
        jsonPath = gltfPointerToJsonPointer(scene, GltfPropertyType::Scenes);
    }
    nlohmann::json operation = R"(
        {
            "dangerous.setValue": {
                "path": "/openScene"
            }
        }
    )"_json;
    operation["dangerous.setValue"]["value"] = jsonPath;

    model::ProjectManager::getInstance()->editorModel.run(operation);
}

void MainWindow::rendererIsReady()
{
    disconnect(m_pRenderer, &GLTFEditorWidget::rendererReady, this, &MainWindow::rendererIsReady);

    InteractorResult result;
    if (Preferences::getInstance()->getInitialFilename().isEmpty()) {
        if (!model::ProjectManager::getInstance()->isCMDMode() && Preferences::getInstance()->getLoadSampleAssetOnStartup()) {
            result = newSampleFile();
        } else {
            GltfCreate createUseCase;
            result = LoadInteractor::getInstance()->execute(createUseCase);
        }
    } else {
        if (model::ProjectManager::getInstance()->isCMDMode() == false) {
            if (openFile(Preferences::getInstance()->getInitialFilename().toStdString())) {
                result = InteractorResult::Success;
            } else {
                result = InteractorResult::Fail;
            }
        } else {
            GltfOpenUseCase useCase { Preferences::getInstance()->getInitialFilename().toStdString() };
            result = LoadInteractor::getInstance()->execute(useCase);
        }
    }

    if (result == InteractorResult::Success) {
        GltfSceneOpenUseCase useCase;
        GltfInteractor::getInstance()->execute(useCase);
    }
}

RenderView* MainWindow::initializeRenderer()
{
    RenderView* pRenderViewDock = new RenderView(this);
    pRenderViewDock->setTitle("Viewport");
    pRenderViewDock->setTitleBar(new CustomTitleWidget(pRenderViewDock->windowTitle()));

    connect(Actions::instance().showViewport, &QAction::triggered, pRenderViewDock, [pRenderViewDock](bool visible) { if (visible) pRenderViewDock->show(); else pRenderViewDock->hide(); });
    connect(pRenderViewDock, &CustomDockWidget::visibilityChanged, Actions::instance().showViewport, &QAction::setChecked);

    m_pRenderer = pRenderViewDock->getRenderer();
    connect(m_pRenderer, &GLTFEditorWidget::rendererReady, this, &MainWindow::rendererIsReady);

    return pRenderViewDock;
}

CustomDockWidget* MainWindow::initializeHierarchy()
{
    Hierarchy* pHierarchyDock = new Hierarchy(this, m_qmlEngine);
    pHierarchyDock->setTitleBar(new CustomTitleWidget(""));

    connect(Actions::instance().showHierarchy, &QAction::triggered, pHierarchyDock, [pHierarchyDock](bool visible) { if (visible) pHierarchyDock->show(); else pHierarchyDock->hide(); });
    connect(pHierarchyDock, &CustomDockWidget::visibilityChanged, Actions::instance().showHierarchy, &QAction::setChecked);

    return pHierarchyDock;
}

CustomDockWidget* MainWindow::initializeInspector()
{
    CustomDockWidget* pInspectorDock = new Inspector(this, m_qmlEngine);
    pInspectorDock->setTitle("Inspector");
    pInspectorDock->setTitleBar(new CustomTitleWidget(pInspectorDock->windowTitle()));

    connect(Actions::instance().showInspector, &QAction::triggered, pInspectorDock, [pInspectorDock](bool visible) { if (visible) pInspectorDock->show(); else pInspectorDock->hide(); });
    connect(pInspectorDock, &CustomDockWidget::visibilityChanged, Actions::instance().showInspector, &QAction::setChecked);

    return pInspectorDock;
}

CustomDockWidget* MainWindow::initializeResources()
{
    CustomDockWidget* pResourcesDock = new Hierarchy(this, m_qmlEngine, true);
    pResourcesDock->setTitle("Resources");
    pResourcesDock->setTitleBar(new CustomTitleWidget(pResourcesDock->windowTitle()));

    connect(Actions::instance().showResources, &QAction::triggered, pResourcesDock, [pResourcesDock](bool visible) { if (visible) pResourcesDock->show(); else pResourcesDock->hide(); });
    connect(pResourcesDock, &CustomDockWidget::visibilityChanged, Actions::instance().showResources, &QAction::setChecked);

    return pResourcesDock;
}

CustomDockWidget* MainWindow::initializeLogger()
{
    CustomDockWidget* pLoggerDock = new Logger(this, m_qmlEngine);
    pLoggerDock->setTitle("Logger");
    pLoggerDock->setTitleBar(new CustomTitleWidget(pLoggerDock->windowTitle()));

    connect(Actions::instance().showLogger, &QAction::triggered, pLoggerDock, [pLoggerDock](bool visible) {
        visible ? pLoggerDock->show() : pLoggerDock->hide();
    });
    connect(pLoggerDock, &CustomDockWidget::visibilityChanged, Actions::instance().showLogger, &QAction::setChecked);

    return pLoggerDock;
}

void MainWindow::changeAlwaysOnTop(QWindow* window)
{
    if (!window->isVisible()) {
        return;
    }
    if (QGuiApplication::applicationState() == Qt::ApplicationActive) {
        window->setFlags(window->flags() | Qt::WindowStaysOnTopHint);
    } else {
        window->setFlags(window->flags() & ~Qt::WindowStaysOnTopHint);
    }
    window->show();
}

void MainWindow::initializePreferences()
{
    // fix for qml file dialog
    m_preferencesWindow = new HotReloadQQuickView(m_qmlEngine);
    m_preferencesWindow->setIcon(QIcon(pathConstants::iconPreferencesTitleBar));
    m_preferencesWindow->setTitle("Preferences");
    m_preferencesWindow->setFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);

    m_preferencesWindow->engine()->rootContext()->setContextProperty("preferences", Preferences::getInstance());

    m_preferencesWindow->setSource(QUrl(QString(UX3D_QML_ROOT_DIR_URL) + "/preferences.qml"));

    m_preferencesWindow->setMaximumWidth(700);
    m_preferencesWindow->setMinimumWidth(700);
    m_preferencesWindow->setMaximumHeight(420);
    m_preferencesWindow->setMinimumHeight(420);

    connect(static_cast<QApplication*>(QApplication::instance()), &QGuiApplication::applicationStateChanged, this, [this]() {
        changeAlwaysOnTop(m_preferencesWindow);
    });

    connect(Actions::instance().preferences, &QAction::triggered, this,
        [this] {
            m_preferencesWindow->reload();
            m_preferencesWindow->show();
            m_preferencesWindow->raise();
        });
}

void MainWindow::initializeMITLicense()
{
    // fix for qml file dialog
    m_mitLicenseWindow = new HotReloadQQuickView(m_qmlEngine);
    m_mitLicenseWindow->setTitle("Open Source License Notice");
    m_mitLicenseWindow->setFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);

    m_mitLicenseWindow->setSource(QUrl(QString(UX3D_QML_ROOT_DIR_URL) + "/Popups/MITLicense.qml"));

    m_mitLicenseWindow->setMaximumWidth(700);
    m_mitLicenseWindow->setMinimumWidth(700);
    m_mitLicenseWindow->setMaximumHeight(450);
    m_mitLicenseWindow->setMinimumHeight(450);

    connect(static_cast<QApplication*>(QApplication::instance()), &QGuiApplication::applicationStateChanged, this, [this]() {
        changeAlwaysOnTop(m_mitLicenseWindow);
    });

    connect(Actions::instance().showMITLicense, &QAction::triggered, [this] {
        m_mitLicenseWindow->show();
    });

    connect(Actions::instance().showPrivacyPolicy, &QAction::triggered, [this] {
        QDesktopServices::openUrl(QUrl(constants::appUri));
    });
    QObject::connect(m_mitLicenseWindow->rootObject(), SIGNAL(close()), this, SLOT(closeMITLicense()));
}

void MainWindow::initializeAboutGestaltor()
{
    const QString appName = constants::appName;

    const QString uri = constants::appUri;
    const QString uriText = "Gestaltor.com";

    const QString versionText = QString::fromStdString(Version::get());

    const QString applicationDataPath = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    const QString qtLicensePath = applicationDataPath + "/Licenses/QtLicenses.txt";

    m_aboutGestaltorWindow = new HotReloadQQuickView(m_qmlEngine);
    m_aboutGestaltorWindow->setTitle("About");
    m_aboutGestaltorWindow->setFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);

    m_aboutGestaltorWindow->rootContext()->setContextProperty("appName", appName);
    m_aboutGestaltorWindow->rootContext()->setContextProperty("uri", uri);
    m_aboutGestaltorWindow->rootContext()->setContextProperty("uriText", uriText);
    m_aboutGestaltorWindow->rootContext()->setContextProperty("versionText", versionText);
    m_aboutGestaltorWindow->rootContext()->setContextProperty("qtLicensePath", qtLicensePath);

    m_aboutGestaltorWindow->setSource(QUrl(QString(UX3D_QML_ROOT_DIR_URL) + "/Popups/AboutGestaltor.qml"));

    m_aboutGestaltorWindow->setMaximumWidth(470);
    m_aboutGestaltorWindow->setMinimumWidth(470);
    m_aboutGestaltorWindow->setMaximumHeight(495);
    m_aboutGestaltorWindow->setMinimumHeight(495);

    connect(static_cast<QApplication*>(QApplication::instance()), &QGuiApplication::applicationStateChanged, this, [this]() {
        changeAlwaysOnTop(m_aboutGestaltorWindow);
    });

    connect(Actions::instance().about, &QAction::triggered, [this] {
        m_aboutGestaltorWindow->show();
    });
    QObject::connect(m_aboutGestaltorWindow->rootObject(), SIGNAL(close()), this, SLOT(closeAboutGestaltor()));
}

void MainWindow::setShowWelcomeOnStartup(bool value)
{
    m_showWelcomeOnStartup = value;
    Preferences::getInstance()->setShowWelcomeOnStartup(m_showWelcomeOnStartup);
}

bool MainWindow::getShowWelcomeOnStartup()
{
    m_showWelcomeOnStartup = Preferences::getInstance()->getShowWelcomeOnStartup();
    return m_showWelcomeOnStartup;
}

void MainWindow::initializeThirdParties()
{
    // fix for qml file dialog
    m_thirdPartiesWindow = new HotReloadQQuickView(m_qmlEngine);
    QString title = "Third - Party Software used by ";
    m_thirdPartiesWindow->setTitle(title + constants::appName);
    m_thirdPartiesWindow->setFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);

    m_thirdPartiesWindow->setSource(QUrl(QString(UX3D_QML_ROOT_DIR_URL) + "/Popups/ThirdParties.qml"));

    connect(static_cast<QApplication*>(QApplication::instance()), &QGuiApplication::applicationStateChanged, this, [this]() {
        changeAlwaysOnTop(m_thirdPartiesWindow);
    });

    connect(Actions::instance().showThirdPartySoftware, &QAction::triggered, [this] {
        m_thirdPartiesWindow->show();
    });
}

void MainWindow::closeMITLicense()
{
    m_mitLicenseWindow->close();
}

void MainWindow::closeValidationPopup()
{
    m_validationAfterSave->close();
}

void MainWindow::closeAboutGestaltor()
{
    m_aboutGestaltorWindow->close();
}

CustomDockWidget* MainWindow::initializeViewportSettings()
{
    ViewportSettings* viewportSettingsDock = new ViewportSettings(this, m_qmlEngine);
    viewportSettingsDock->setTitle("Viewport Settings");
    viewportSettingsDock->setTitleBar(new CustomTitleWidget(viewportSettingsDock->windowTitle()));

    connect(Actions::instance().showViewportSettings, &QAction::triggered, viewportSettingsDock, [viewportSettingsDock](bool visible) { if (visible) viewportSettingsDock->show(); else viewportSettingsDock->hide(); });
    connect(viewportSettingsDock, &CustomDockWidget::visibilityChanged, Actions::instance().showViewportSettings, &QAction::setChecked);
    connect(viewportSettingsDock, &ViewportSettings::environmentMapSelected, m_pRenderer, &GLTFRendererWidget::updateEnvironmentMap);

    return viewportSettingsDock;
}

CustomDockWidget* MainWindow::initializeStatistics()
{
    StatisticsWidget* pStatisticsDock = new StatisticsWidget(this, m_qmlEngine);
    pStatisticsDock->setTitle("Statistics");
    CustomTitleWidget* titleBar = new CustomTitleWidget(pStatisticsDock->windowTitle());
    pStatisticsDock->setTitleBar(titleBar);

    connect(
        Actions::instance().showStatistics,
        &QAction::triggered,
        pStatisticsDock,
        [pStatisticsDock](bool visible) {
            if (visible) {
                pStatisticsDock->show();
            } else {
                pStatisticsDock->hide();
            }
        });

    connect(pStatisticsDock, &CustomDockWidget::visibilityChanged, Actions::instance().showStatistics, &QAction::setChecked);

    return pStatisticsDock;
}

void MainWindow::initializeLayout()
{
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCentralWidget(m_pRendererDock);

    m_pHierarchyDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::LeftDockWidgetArea, m_pHierarchyDock);

    m_pInspectorDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::RightDockWidgetArea, m_pInspectorDock);

    m_pResourcesDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::LeftDockWidgetArea, m_pResourcesDock);

    m_pViewportSettingsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::BottomDockWidgetArea, m_pViewportSettingsDock);

    m_pLoggerDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::BottomDockWidgetArea, m_pLoggerDock);

    m_pStatisticsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::BottomDockWidgetArea, m_pStatisticsDock);

    resizeDocks({ m_pInspectorDock }, { 590 }, Qt::Horizontal);
    resizeDocks({ m_pHierarchyDock, m_pResourcesDock }, { 360, 300 }, Qt::Horizontal);
    resizeDocks({ m_pLoggerDock, m_pStatisticsDock, m_pViewportSettingsDock }, { 200, 200, 200 }, Qt::Horizontal);
    resizeDocks({ m_pLoggerDock, m_pViewportSettingsDock }, { 300, 300 }, Qt::Vertical);
}

void MainWindow::setupMenuBarActions()
{
    menuFile->setToolTipsVisible(true);
    menuFile->addAction(Actions::instance().newFile);
    menuFile->addAction(Actions::instance().newSampleFile);
    menuFile->addSeparator();
    menuFile->addAction(Actions::instance().open);
    menuFile->addAction(Actions::instance().openRecent);
    menuFile->addSeparator();
    menuFile->addAction(Actions::instance().save);
    menuFile->addAction(Actions::instance().saveAs);
    menuFile->addAction(Actions::instance().saveAsMinified);
    menuFile->addSeparator();
    menuFile->addAction(Actions::instance().importImage);
    menuFile->addAction(Actions::instance().importTexture);
    menuFile->addAction(Actions::instance().importMaterial);
    menuFile->addAction(Actions::instance().importXmpPacket);
    menuFile->addAction(Actions::instance().import3DModel);
    // import Audio Source is Gestaltor 1.0 functionality not yet available in Gestaltor 2.0
    // menuFile->addAction(Actions::instance().importAudioSource);
    menuFile->addSeparator();
    menuFile->addAction(Actions::instance().exportUSDZ);
    menuFile->addSeparator();
    menuFile->addAction(Actions::instance().quit);

    menuEdit->setToolTipsVisible(true);
    menuEdit->addAction(Actions::instance().undo);
    menuEdit->addAction(Actions::instance().redo);
    menuEdit->addSeparator();
    menuEdit->addAction(Actions::instance().copy);
    menuEdit->addAction(Actions::instance().pasteLinked);
    menuEdit->addAction(Actions::instance().pasteDeep);
    menuEdit->addAction(Actions::instance().cut);
    menuEdit->addAction(Actions::instance().remove);
    menuEdit->addSeparator();
    menuEdit->addAction(Actions::instance().preferences);

    menuCreate->setToolTipsVisible(true);
    menuCreate->addAction(Actions::instance().createScene);
    menuCreate->addAction(Actions::instance().createNode);
    menuCreate->addAction(Actions::instance().createCamera);
    // menuCreate->addAction(Actions::instance().createAudioEmitter);
    menuCreate->addAction(Actions::instance().createLight);
    menuCreate->addAction(Actions::instance().createMesh);
    menuCreate->addAction(Actions::instance().createMaterial);
    menuCreate->addAction(Actions::instance().createMaterialVariant);
    menuCreate->addAction(Actions::instance().createTexture);
    menuCreate->addAction(Actions::instance().createSampler);
    menuCreate->addAction(Actions::instance().createAnimation);
    menuCreate->addSeparator();
    menuCreate->addAction(Actions::instance().createCameraFromView);

    menuViewport->setToolTipsVisible(true);
    menuViewport->addAction(Actions::instance().editMode);
    menuViewport->addAction(Actions::instance().renderMode);
    menuViewport->addSeparator();
    menuViewport->addAction(Actions::instance().toolSelect);
    menuViewport->addAction(Actions::instance().toolMove);
    menuViewport->addAction(Actions::instance().toolRotate);
    menuViewport->addAction(Actions::instance().toolScale);
    menuViewport->addSeparator();
    menuViewport->addAction(Actions::instance().editSpaceWorld);
    menuViewport->addAction(Actions::instance().editSpaceLocal);
    menuViewport->addSeparator();
    menuViewport->addAction(Actions::instance().showGrid);
    menuViewport->addSeparator();
    menuViewport->addAction(Actions::instance().showBoundingBoxes);
    menuViewport->addAction(Actions::instance().showSkinsAndBones);
    m_visualizationModeMenu = menuViewport->addMenu("Visualization Mode");
    m_visualizationModeMenu->addAction(Actions::instance().visualizationModeDefaultShaded);
    m_visualizationModeMenu->addAction(Actions::instance().visualizationModeWireframe);
    m_visualizationModeMenu->addSeparator();
    m_visualizationModeMenu->addActions(Actions::instance().visualizationModeDebugShadings);
    connect(Actions::instance().editMode, &QAction::changed, [this] { m_visualizationModeMenu->setDisabled(false); });
    menuViewport->addSeparator();
    menuViewport->addAction(Actions::instance().focus);
    menuViewport->addSeparator();
    auto animationMenu = menuViewport->addMenu("Animation");
    animationMenu->addAction(Actions::instance().playingAnimation);
    animationMenu->addAction(Actions::instance().stopAnimation);
    animationMenu->addAction(Actions::instance().rewindAnimation);
    animationMenu->addAction(Actions::instance().forwardAnimation);
    menuViewport->addSeparator();
    auto cameraProjectionMenu = menuViewport->addMenu("Camera Projection");
    cameraProjectionMenu->addAction(Actions::instance().projectionPerspective);
    cameraProjectionMenu->addAction(Actions::instance().projectionOrthographic);
    auto cameraOrientationMenu = menuViewport->addMenu("Camera Orientation");
    cameraOrientationMenu->addAction(Actions::instance().cameraOrientationTop);
    cameraOrientationMenu->addAction(Actions::instance().cameraOrientationBottom);
    cameraOrientationMenu->addAction(Actions::instance().cameraOrientationRight);
    cameraOrientationMenu->addAction(Actions::instance().cameraOrientationLeft);
    cameraOrientationMenu->addAction(Actions::instance().cameraOrientationFront);
    cameraOrientationMenu->addAction(Actions::instance().cameraOrientationBack);
    menuViewport->addSeparator();
    menuViewport->addAction(Actions::instance().viewportResolution);
    menuViewport->addAction(Actions::instance().renderCapture);

    menuView->setToolTipsVisible(true);
    menuView->addAction(Actions::instance().showInspector);
    menuView->addAction(Actions::instance().showHierarchy);
    menuView->addAction(Actions::instance().showViewport);
    menuView->addAction(Actions::instance().showResources);
    menuView->addAction(Actions::instance().showLogger);
    menuView->addAction(Actions::instance().showViewportSettings);
    menuView->addAction(Actions::instance().showStatistics);
    menuView->addSeparator();
    menuView->addAction(Actions::instance().resetLayout);
    menuView->addSeparator();

    menuAutomation->setToolTipsVisible(true);

    menuAutomation->addAction(Actions::instance().applyDracoToGlTF);
    menuAutomation->addAction(Actions::instance().removeDracoFromGlTF);
    menuAutomation->addAction(Actions::instance().removeMeshQuantizationFromGlTF);
    menuAutomation->addSeparator();
    auto removeAllExtensionsMenu = menuAutomation->addMenu("Remove Extensions");
    auto removeAllMaterialExtensionsMenu = removeAllExtensionsMenu->addMenu("Material");
    removeAllMaterialExtensionsMenu->addAction(Actions::instance().removeSpecular);
    removeAllMaterialExtensionsMenu->addAction(Actions::instance().removeClearcoat);
    removeAllMaterialExtensionsMenu->addAction(Actions::instance().removeSheen);
    removeAllMaterialExtensionsMenu->addAction(Actions::instance().removeTransmission);
    removeAllMaterialExtensionsMenu->addAction(Actions::instance().removeVolume);
    removeAllMaterialExtensionsMenu->addAction(Actions::instance().removeIOR);
    removeAllMaterialExtensionsMenu->addAction(Actions::instance().removeSpecularGlossiness);
    removeAllMaterialExtensionsMenu->addAction(Actions::instance().removeUnlit);
    removeAllMaterialExtensionsMenu->addAction(Actions::instance().removeMaterialVariants);
    removeAllMaterialExtensionsMenu->addAction(Actions::instance().removeIridescence);
    removeAllMaterialExtensionsMenu->addAction(Actions::instance().removeEmissiveStrength);
    auto removeAllTextureExtensionsMenu = removeAllExtensionsMenu->addMenu("Texture");
    removeAllTextureExtensionsMenu->addAction(Actions::instance().removeTextureTransform);
    auto removeAllLightExtensionsMenu = removeAllExtensionsMenu->addMenu("Light");
    removeAllLightExtensionsMenu->addAction(Actions::instance().removeLights);
    removeAllExtensionsMenu->addAction(Actions::instance().removeUnknownExtensions);
    auto removeAllContentMenu = menuAutomation->addMenu("Remove Properties");
    removeAllContentMenu->addAction(Actions::instance().removeExtras);
    removeAllContentMenu->addAction(Actions::instance().removeNames);
    menuAutomation->addAction(Actions::instance().cleanExtensionsUsed);
    menuAutomation->addSeparator();

    menuAutomation->addAction(Actions::instance().cleanup);
    auto removeAllUnusedMenu = menuAutomation->addMenu("Cleanup by Type");
    removeAllUnusedMenu->addAction(Actions::instance().removeUnusedNodes);
    removeAllUnusedMenu->addAction(Actions::instance().removeUnusedTexcoordAttributes);

    // if (!Version::get().isDevelopmentVersion()) {
    //     menubar->removeAction(menuDeveloper->menuAction());
    // } else {
    //     menuDeveloper->setToolTipsVisible(true);
    //     menuDeveloper->addAction(Actions::instance().reloadStyleSheet);
    //     menuDeveloper->addAction(Actions::instance().reloadStyleSheetFromDisk);
    // }
    menubar->removeAction(menuDeveloper->menuAction());

    menuHelp->setToolTipsVisible(true);
    menuHelp->addAction(Actions::instance().about);
    menuHelp->addAction(Actions::instance().showThirdPartySoftware);
    menuHelp->addAction(Actions::instance().showPrivacyPolicy);
    menuHelp->addAction(Actions::instance().showMITLicense);
    menuHelp->addSeparator();
    menuHelp->addSeparator();
    menuHelp->addSeparator();
    menuHelp->addAction(Actions::instance().openDocumentation);

    connect(Actions::instance().quit, &QAction::triggered, this, &MainWindow::close);
    connect(Actions::instance().open, &QAction::triggered, this, &MainWindow::open);
    connect(Actions::instance().newSampleFile, &QAction::triggered, this, &MainWindow::newSampleFile);
    connect(Actions::instance().newFile, &QAction::triggered, this, &MainWindow::newFile);
    connect(Actions::instance().save, &QAction::triggered, this, &MainWindow::save);
    connect(Actions::instance().saveAs, &QAction::triggered, this, &MainWindow::saveAs);
    connect(Actions::instance().saveAsMinified, &QAction::triggered, this, &MainWindow::saveAsMinified);
    connect(Actions::instance().resetLayout, &QAction::triggered, this, &MainWindow::restoreDefaultGeometry);
    connect(Actions::instance().reloadStyleSheet, &QAction::triggered, this, &MainWindow::loadStyleSheet);
    connect(Actions::instance().reloadStyleSheetFromDisk, &QAction::triggered, this, &MainWindow::loadStyleSheetFromDisk);

    connect(Actions::instance().exportUSDZ, &QAction::triggered, this, &MainWindow::exportUSDZ);

    connect(Actions::instance().undo, &QAction::triggered, this, &MainWindow::undo);
    connect(Actions::instance().redo, &QAction::triggered, this, &MainWindow::redo);

    connect(Actions::instance().removeUnknownExtensions, &QAction::triggered, [] {
        auto taskDescription = R"(
        {
            "remove_unknown_extension": {
            }
        }
        )"_json;

        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription), "", true);
    });

    connect(Actions::instance().removeUnusedTexcoordAttributes, &QAction::triggered, [] {
        auto taskDescription = R"(
        {
            "clean_texture_coordinate_attributes": {
            }
        }
        )"_json;

        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription), "", true);
    });
    connect(Actions::instance().cleanup, &QAction::triggered, [] {
        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule({ { "clean_gltf", nlohmann::json::object() } }), "", true);
    });
    connect(Actions::instance().removeUnusedNodes, &QAction::triggered, [] {
        auto taskDescription = R"(
        {
            "clean_nodes": {
            }
        }
        )"_json;

        QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription), "", true);
    });

    Actions::instance().openRecent->setMenu(&m_recentFileHelper->getRecentFilesMenu());
    QMenu* recentMenu = &m_recentFileHelper->getRecentFilesMenu();
    connect(&m_recentFileHelper->getRecentFilesMenu(), &QMenu::triggered, this,
        [this, recentMenu](QAction* action) { openFileRecent(static_cast<int>(recentMenu->actions().indexOf(action))); });
    connect(menuFile, &QMenu::aboutToShow, [this] { m_recentFileHelper->update(); });
}

void MainWindow::updateWindowTitle(const QString& fileName)
{
    QString windowTitle;

    windowTitle.append(constants::appName);
    windowTitle.append(" ");
    windowTitle.append(constants::releaseVersion);

    windowTitle.append(" - ");

    windowTitle.append("[*]");

    windowTitle.append(fileName.isEmpty() ? getFileDisplayName() : fileName);

    setWindowTitle(windowTitle);
}

void MainWindow::dropEvent(QDropEvent* _pEvent)
{
    if (!_pEvent || !_pEvent->mimeData()->hasUrls()) {
        return;
    }

    QList<QUrl> urlList = _pEvent->mimeData()->urls();

    if (urlList.empty()) {
        return;
    }
    auto lambda = [this, urlList]() {
        std::optional<filesystem::File> droppedGltfFile;

        for (auto url : urlList) {
            filesystem::Path droppedPath(QtFileSystem::instance(), url.toLocalFile().toStdString());
#ifdef UX3D_IS_APPLE
            // Dropping counts as user interaction for the apple application sandbox, so we should
            // store the bookmark so that we have access to the ressource in the future
            QtFileSystem::sandboxSaveBookmark(droppedPath);
#endif
            if (droppedPath.exists() == false) {
                continue;
            }

            // Search for a glTF file in the dropped files or directories
            if (droppedPath.isFile()) {
                filesystem::File droppedFile = droppedPath;
                if (droppedFile.getExtension() == ".gltf" || droppedFile.getExtension() == ".glb" || droppedFile.getExtension() == ".gestalt") {
                    droppedGltfFile = droppedFile;
                    break;
                }
            } else if (droppedPath.isDirectory()) {
                filesystem::Dir droppedDir = droppedPath;
                for (auto file : droppedDir.listFiles()) {
                    if (file.getExtension() == ".gltf" || file.getExtension() == ".glb" || file.getExtension() == ".gestalt") {
                        droppedGltfFile = file;
                        break;
                    }
                }
            }
        }

        if (!droppedGltfFile.has_value()) {
            return;
        }

#ifdef UX3D_IS_APPLE
        filesystem::IFileSystem::sandboxRequestAccess(droppedGltfFile.value());
#endif

        QString fileName = QString::fromStdString(droppedGltfFile.value().toString());

        // Required to check unicode and ensure utf8 compatibility
        std::string utf8 = fileName.toUtf8().constData();
        std::filesystem::path utf8_path((const char8_t*)utf8.c_str());
        try {
            std::string fileName = utf8_path.string();
            openFile(fileName);
        } catch (...) {
            core::logError("Unable to open unicode file path");
            QMLCard::error("Unable to open unicode file path");
        }
    };
    this->disconnect(SIGNAL(continueTaskAfterPrompt()));
    connect(this, &MainWindow::continueTaskAfterPrompt, this, lambda, Qt::SingleShotConnection);
    askToSaveBeforeLoosingState();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* _pEvent)
{
    if (_pEvent->mimeData()->hasUrls()) {
        _pEvent->acceptProposedAction();
    }
}

void MainWindow::closeEvent(QCloseEvent* _pEvent)
{
    std::cout << "closing..." << std::endl;
    if (!m_closeApp) {
        _pEvent->ignore();
    }
    auto lambda = [this]() {
        saveSession();
        m_closeApp = true;
        close();
        emit exited();
    };
    this->disconnect(SIGNAL(continueTaskAfterPrompt()));
    connect(this, &MainWindow::continueTaskAfterPrompt, this, lambda, Qt::SingleShotConnection);
    askToSaveBeforeLoosingState();
}

void MainWindow::open()
{
    auto lambda = [this]() {
        QString dialogCaption;
        dialogCaption = "Open";

        const QString applicationDataPath = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
        QSettings userSettings(applicationDataPath + "/userConfig.ini", QSettings::IniFormat);
        QString location = userSettings.value("LastLoadedGltf").toString();
        if (location.isEmpty()) {
            location = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        }

        const QString filter = openFileFiltersToString(OpenFileFilter::AllModelFiles);

        QFileDialog* fileDialog = new QFileDialog(this, dialogCaption, location, filter);
        fileDialog->setLabelText(QFileDialog::Accept, dialogCaption);
        fileDialog->setFileMode(QFileDialog::ExistingFile);

#ifdef UX3D_IS_APPLE
        fileDialog->setModal(false);
#else
        fileDialog->setModal(true); // force native dialog on windows
#endif

        connect(fileDialog, &QFileDialog::accepted, [this, fileDialog] {
            const QStringList& fileNames = fileDialog->selectedFiles();
            if (fileNames.empty()) {
                return;
            }
            for (auto fileName : fileNames) {
                filesystem::Path path(QtFileSystem::instance(), fileName.toStdString());
                QtFileSystem::sandboxSaveBookmark(path);
            }
            QString fileName = fileNames.at(0);

#ifdef UX3D_IS_APPLE
            filesystem::Path targetDirectory = QtFileSystem::instance().toFile(fileName.toStdString()).getParent();
            filesystem::IFileSystem::sandboxRequestAccess(targetDirectory);
#endif

            // Required to check unicode and ensure utf8 compatibility
            std::string utf8 = fileName.toUtf8().constData();
            std::filesystem::path utf8_path((const char8_t*)utf8.c_str());
            try {
                std::string fileName = utf8_path.string();
                openFile(fileName);
            } catch (...) {
                core::logError("Unable to open unicode file path");
                QMLCard::error("Unable to open unicode file path");
            }
        });

        fileDialog->show();
    };

    this->disconnect(SIGNAL(continueTaskAfterPrompt()));
    connect(this, &MainWindow::continueTaskAfterPrompt, this, lambda, Qt::SingleShotConnection);
    askToSaveBeforeLoosingState();
}

bool MainWindow::openFile(std::string path)
{

    if (!path.empty()) {
        model::ProjectManager::getInstance()->model.clearHistory();
        InteractorResult result;
        GltfOpenUseCase openUseCase { path };
        result = LoadInteractor::getInstance()->execute(openUseCase);

        if (result == InteractorResult::Success) {
            GltfSceneOpenUseCase useCase;
            GltfInteractor::getInstance()->execute(useCase);
            model::ProjectManager::getInstance()->setGltfPath(path);
            model::ProjectManager::getInstance()->stateHistory.cleanseGltf();
            QString title = QString::fromUtf8(path);
            updateWindowFromOpenedFile(title);
            return true;
        }
    }
    return false;
}

void MainWindow::updateWindowFromOpenedFile(const QString& path)
{
    std::filesystem::path stdPath(path.toStdString());
    auto qtitle = QString::fromStdString(stdPath.filename().string());
    updateWindowTitle(qtitle);
    checkRequiredExtensions();
    m_recentFileHelper->addToFileQueue(path);

    const QString applicationDataPath = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    QSettings userSettings(applicationDataPath + "/userConfig.ini", QSettings::IniFormat);
    userSettings.setValue("LastLoadedGltf", path);
}

void MainWindow::openByFileName(QString& _filename)
{
    // ToDo Refactor with openSample
    auto lambda = [this, _filename]() {
        openFile(_filename.toLocal8Bit().constData());
    };
    this->disconnect(SIGNAL(continueTaskAfterPrompt()));
    connect(this, &MainWindow::continueTaskAfterPrompt, this, lambda, Qt::SingleShotConnection);
    askToSaveBeforeLoosingState();
}

InteractorResult MainWindow::newSampleFile()
{
    auto lambda = [this]() {
        // Save the sample asset to App Data, so that it can be
        // loaded by an operation (which doesn't use Qt File API)
        QString applicationDataPath = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
        auto fileName = applicationDataPath + "/sample.glb";
        if (!QFile::exists(fileName)) {
            QFile::copy(pathConstants::glTFSampleAsset, fileName);
        }

        const GltfOpenUseCase openUseCase { fileName.toStdString() };
        const InteractorResult result = LoadInteractor::getInstance()->execute(openUseCase);

        if (result == InteractorResult::Success) {
            GltfSceneOpenUseCase useCase;
            GltfInteractor::getInstance()->execute(useCase);

            // Explicitly overwrite sample asset path
            model::ProjectManager::getInstance()->resetPaths();

            model::ProjectManager::getInstance()->stateHistory.cleanseGltf();
            updateWindowTitle();
        }
        return result;
    };
    this->disconnect(SIGNAL(continueTaskAfterPrompt()));
    connect(this, &MainWindow::continueTaskAfterPrompt, this, lambda, Qt::SingleShotConnection);
    askToSaveBeforeLoosingState();
    // This is not really good. The call is now async so this function should not return anything
    return InteractorResult::Fail;
}

void MainWindow::exportUSDZ()
{
    SaveInteractor::getInstance()->exportToUSD();
}

void MainWindow::newFile()
{
    auto lambda = [this]() {
        GltfCreate createUseCase;
        createUseCase.createNewScene = true;
        InteractorResult result = LoadInteractor::getInstance()->execute(createUseCase);
        updateWindowTitle();
    };
    this->disconnect(SIGNAL(continueTaskAfterPrompt()));
    connect(this, &MainWindow::continueTaskAfterPrompt, this, lambda, Qt::SingleShotConnection);
    askToSaveBeforeLoosingState();
}

bool MainWindow::save()
{
    if (!m_dirty) {
        return true;
    }

    bool successfullySaved = SaveInteractor::getInstance()->saveProject(AskForLocation::IfNoPathSet, false) == InteractorResult::Success;
    if (successfullySaved) {
        model::ProjectManager::getInstance()->stateHistory.cleanseGltf();
        std::string filename = model::ProjectManager::getInstance()->getGltfPath();
        QString qFilename = QString::fromStdString(filename);
        m_recentFileHelper->addToFileQueue(qFilename);
        showValidationPopup(false);
    }
    return successfullySaved;
}

void MainWindow::saveAs()
{
    InteractorResult saveResult = SaveInteractor::getInstance()->saveProject(AskForLocation::Always, false);
    std::string filename = model::ProjectManager::getInstance()->getGltfPath();
    QString qFilename = QString::fromStdString(filename);
    if (saveResult == InteractorResult::Success) {
        model::ProjectManager::getInstance()->stateHistory.cleanseGltf();
        m_recentFileHelper->addToFileQueue(qFilename);
        showValidationPopup(false);
        updateWindowTitle();
    }
}

void MainWindow::saveAsMinified()
{
    InteractorResult saveResult = SaveInteractor::getInstance()->saveProject(AskForLocation::Always, true);
    std::string filename = model::ProjectManager::getInstance()->getGltfPath();
    QString qFilename = QString::fromStdString(filename);
    if (saveResult == InteractorResult::Success) {
        model::ProjectManager::getInstance()->stateHistory.cleanseGltf();
        m_recentFileHelper->addToFileQueue(qFilename);
        showValidationPopup(false);
        updateWindowTitle();
    }
}

void MainWindow::restoreSession()
{
    QString applicationDataPath = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    QSettings userSettings(applicationDataPath + "/lastSessionResources.ini", QSettings::IniFormat);

    userSettings.beginGroup("MainWindow");
    if (userSettings.contains("geometry")) {
        restoreGeometry(userSettings.value("geometry").toByteArray());
    } else {
        restoreDefaultGeometry();
    }
    if (isMaximized()) {
        QScreen* screen = QGuiApplication::primaryScreen();
        setGeometry(screen->availableGeometry());
    }
    if (userSettings.contains("windowState")) {
        restoreState(userSettings.value("windowState").toByteArray());
    }
    userSettings.endGroup();

    initializeViewActions();
}

void MainWindow::restoreDefaultGeometry()
{
    m_pHierarchyDock->setFloating(false);
    m_pHierarchyDock->show();

    m_pRendererDock->setFloating(false);
    m_pRendererDock->show();

    m_pInspectorDock->setFloating(false);
    m_pInspectorDock->show();

    m_pResourcesDock->setFloating(false);
    m_pResourcesDock->show();

    m_pLoggerDock->setFloating(false);
    m_pLoggerDock->hide();

    m_pViewportSettingsDock->setFloating(false);
    m_pViewportSettingsDock->show();

    m_pStatisticsDock->setFloating(false);
    m_pStatisticsDock->hide();

    initializeLayout();
}

void MainWindow::initializeViewActions()
{
    Actions::instance().showHierarchy->setChecked(m_pHierarchyDock->isVisible());
    Actions::instance().showViewport->setChecked(m_pRendererDock->isVisible());
    Actions::instance().showInspector->setChecked(m_pInspectorDock->isVisible());
    Actions::instance().showResources->setChecked(m_pResourcesDock->isVisible());
    Actions::instance().showLogger->setChecked(m_pLoggerDock->isVisible());
    Actions::instance().showViewportSettings->setChecked(m_pViewportSettingsDock->isVisible());
    Actions::instance().showStatistics->setChecked(m_pStatisticsDock->isVisible());
}

void MainWindow::saveSession()
{
    QString applicationDataPath = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    QSettings userSettings(applicationDataPath + "/lastSessionResources.ini", QSettings::IniFormat);

    userSettings.beginGroup("MainWindow");
    userSettings.setValue("geometry", saveGeometry());
    userSettings.setValue("windowState", saveState());
    userSettings.endGroup();

    QSettings userConfig(applicationDataPath + "/userConfig.ini", QSettings::IniFormat);
    m_recentFileHelper->storeRecentFilesInUserConfig(userConfig);
}

void MainWindow::loadStyleSheet()
{
    QString globalStyle = "";

    QFile styleFile(UX3D_EDITOR_RESOURCES_DIR "/style_sheets/editor.css");
    if (styleFile.open(QIODevice::ReadOnly)) {
        globalStyle = styleFile.readAll();

        styleFile.close();
    } else {
        core::logWarning("Could not read global editor style from '%s'", styleFile.fileName().toStdString().c_str());
    }

    QString globalVariables = "";

    QFile variablesFile(UX3D_EDITOR_RESOURCES_DIR "/style_sheets/globals.txt");
    if (variablesFile.open(QIODevice::ReadOnly)) {
        globalVariables = variablesFile.readAll();

        variablesFile.close();
    } else {
        core::logWarning("Could not read global editor style from '%s'", variablesFile.fileName().toStdString().c_str());
    }

    globalStyle = utils::applyGlobalsToStyle(globalStyle, globalVariables);

    static_cast<QApplication*>(QCoreApplication::instance())->setStyleSheet(globalStyle);
}

void MainWindow::loadStyleSheetFromDisk()
{
    QString globalStyle = "";

    QFile styleFile(QCoreApplication::applicationDirPath() + "/editor.css");
    if (styleFile.open(QIODevice::ReadOnly)) {
        globalStyle = styleFile.readAll();

        styleFile.close();
    } else {
        core::logWarning("Could not read global editor style from '%s'", styleFile.fileName().toStdString().c_str());
    }

    QString globalVariables = "";

    QFile variablesFile(QCoreApplication::applicationDirPath() + "/globals.txt");
    if (variablesFile.open(QIODevice::ReadOnly)) {
        globalVariables = variablesFile.readAll();

        variablesFile.close();
    } else {
        core::logWarning("Could not read global editor style from '%s'", variablesFile.fileName().toStdString().c_str());
    }

    globalStyle = utils::applyGlobalsToStyle(globalStyle, globalVariables);

    static_cast<QApplication*>(QCoreApplication::instance())->setStyleSheet(globalStyle);
}

void MainWindow::openFileRecent(int _fileIndex)
{
    if (_fileIndex == m_recentFileHelper->getRecentFilesMenu().actions().size() - 1) {
        m_recentFileHelper->clear();
    } else {
        QString filename = m_recentFileHelper->getFilenameByIndex(_fileIndex);
        core::logInfo(("Loading recent file " + filename).toStdString().c_str());
        m_recentFileHelper->moveRecentFileToTop(_fileIndex);
        openByFileName(filename);
    }
}

void MainWindow::checkRequiredExtensions()
{
    const gltf::Gltf& gltf = model::ProjectManager::getInstance()->getGltf();
    if (gltf.containsUnknownRequiredExtensions()) {
        const char* title = "Unsupported extensions";
        const char* message = "The loaded glTF requires extensions that are not supported by Gestaltor.\nGestaltor might be unable to correctly display, edit and save the file.";
        core::logWarning(message);
        QMessageBox::warning(this, title, message);
    }
}

void MainWindow::showValidationPopup(bool askUser)
{
    if (Preferences::getInstance()->getValidateAfterSave()) {
        std::string lastPath = model::ProjectManager::getInstance()->getGltfPath();
        nlohmann::json report = reactor::validation::validate(lastPath);
        if (m_validationAfterSave && (report.value("/issues/numErrors"_json_pointer, 0) > 0 || report.value("/issues/numWarnings"_json_pointer, 0) > 0)) {
            m_validationAfterSave->setDetailedText(QString::fromStdString(report.at("issues").dump(4)));
            m_validationAfterSave->setAskUser(askUser);
            QString text = "The saved glTF has validation issues.";
            if (askUser) {
                text += " Continue?";
            }
            m_validationAfterSave->setText(text);
            connect(
                m_validationAfterSave, &QMLMessageBox::accepted, this, [this]() { emit continueTaskAfterPrompt(); }, Qt::SingleShotConnection);
            m_validationAfterSave->show();
        } else {
            emit continueTaskAfterPrompt();
        }
    } else {
        emit continueTaskAfterPrompt();
    }
}

// askToSaveBeforeLoosingState asks the user to save the file. It returns true if the
// next task where state will be lost can be executed with user agreement
void MainWindow::askToSaveBeforeLoosingState()
{
    if (m_dirty) {
        m_validationAfterSave->disconnect(this);
        QMessageBox* messageBox = new QMessageBox(this);
        messageBox->setWindowTitle("FILES HAVE BEEN MODIFIED");
        messageBox->setText("Save changes to \"" + getFileDisplayName() + "\" before closing?");
        messageBox->setIcon(QMessageBox::Icon::Warning);
        messageBox->setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        messageBox->setDefaultButton(QMessageBox::Save);
        messageBox->setAttribute(Qt::WA_DeleteOnClose);
        connect(messageBox, &QMessageBox::buttonClicked, this, [messageBox, this](QAbstractButton* button) {
            auto standardButton = messageBox->standardButton(button);
            if (standardButton == QMessageBox::Save) {
                bool saveSuccessful = save();
                if (saveSuccessful) {
                    showValidationPopup(true);
                }
            } else if (standardButton != QMessageBox::Cancel) {
                emit continueTaskAfterPrompt();
            }
            messageBox->close();
            messageBox->deleteLater();
        });
        messageBox->show();
    } else {
        emit continueTaskAfterPrompt();
    }
}

void MainWindow::undo()
{
    // TODO test if handling selectd MV is even needed or if renderview cameras should also be handled here
    int currentScene = model::ProjectManager::getInstance()->getCurrentSceneIndex();
    std::optional<uint32_t> selectedMaterialVariant = model::ProjectManager::getInstance()->getSelectedMaterialVariant();

    model::ProjectManager::getInstance()->model.undo();

    model::ProjectManager::getInstance()->setCurrentSceneByIndex(currentScene);
    model::ProjectManager::getInstance()->setSelectedMaterialVariant(selectedMaterialVariant);
    editor::ExternalEditingManager::getInstance()->unregister();
}

void MainWindow::redo()
{
    int currentScene = model::ProjectManager::getInstance()->getCurrentSceneIndex();
    std::optional<uint32_t> selectedMaterialVariant = model::ProjectManager::getInstance()->getSelectedMaterialVariant();

    model::ProjectManager::getInstance()->model.redo();

    model::ProjectManager::getInstance()->setCurrentSceneByIndex(currentScene);
    model::ProjectManager::getInstance()->setSelectedMaterialVariant(selectedMaterialVariant);
}

QString MainWindow::getFileDisplayName()
{
    std::string fileName = model::ProjectManager::getInstance()->getGltfFileName();

    if (!fileName.empty()) {
        return QString::fromStdString(fileName);
    } else {
        return QString(constants::unnamedGltfName);
    }
}

void MainWindow::removePropertyFromGltfHelper(std::string property)
{
    auto taskDescription = R"(
        {
            "remove_properties_by_name": {

            }
        }
        )"_json;
    taskDescription["/remove_properties_by_name/property"_json_pointer] = property;
    model::ProjectManager::getInstance()->model.schedule(taskDescription);
}

void MainWindow::removeKnownExtensionFromGltfHelper(std::string property)
{
    auto taskDescription = R"(
        {
            "remove_extension": {

            }
        }
        )"_json;
    taskDescription["/remove_extension/property"_json_pointer] = property;
    model::ProjectManager::getInstance()->model.schedule(taskDescription);
}

MainWindow* MainWindow::getMainWindow()
{
    return m_instance;
}

void MainWindow::environmentCompatibiliyCheck()
{
    if (Preferences::getInstance()->getEnvironmentsVersion() < 1) {
        QString environmentsLocation = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first() + "/environments";
        QDir dir(environmentsLocation);
        bool environmentsExist = false;
        for (auto& element : dir.entryInfoList()) {
            const core::string name = element.fileName().toStdString();
            if (name != "." && name != "..") {
                environmentsExist = true;
                break;
            }
        }

        if (environmentsExist) {
            QMessageBox* msgBox = new QMessageBox(this);
            msgBox->setWindowTitle("Environments Information");
            msgBox->setText("The environment storage was updated. Old environments won't show correctly. Do you wish to delete them?");
            msgBox->setIconPixmap(QIcon(pathConstants::iconConsoleMessage).pixmap(64, 64));
            QAbstractButton* bDelete = msgBox->addButton("Delete", QMessageBox::YesRole);
            QAbstractButton* bSkip = msgBox->addButton("Skip", QMessageBox::NoRole);
            QAbstractButton* bPermanentSkip = msgBox->addButton("Skip and never remind again", QMessageBox::NoRole);
            msgBox->exec();

            if (bDelete == msgBox->clickedButton()) {
                dir.removeRecursively();
                dir.mkdir(".");
                core::logInfo("Cleaned old environments");
                Preferences::getInstance()->setEnvironmentsVersion(1);
            } else if (bSkip == msgBox->clickedButton()) {
                // Nothing
            } else if (bPermanentSkip == msgBox->clickedButton()) {
                Preferences::getInstance()->setEnvironmentsVersion(1);
            }
        } else {
            Preferences::getInstance()->setEnvironmentsVersion(1);
        }
    }
}
