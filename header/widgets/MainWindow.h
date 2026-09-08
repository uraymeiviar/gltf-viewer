#pragma once

#include "ApplicationData.h"
#include "GltfPropertyPointer.h"
#include "Preferences.h"
#include "core/String.h"
#include "helpers/CommandLineParsing.h"
#include "interfaces/GltfInteractorOutput.h"
#include "interfaces/LoadInteractorOutput.h"
#include "widgets/CustomDockWidget.h"

#include "enums/InteractorResult.h"

#include "ui_mainwindow.h"

#include <QMainWindow>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

class QSettings;
class QQmlEngine;
class QQuickCloseEvent;
class QMessageBox;

namespace ux3d::gltf {
class Scene;
}

namespace ux3d::editor::utils {
class RecentFilesHelper;
}

namespace ux3d::editor {
class HotReloadQQuickView;
class GLTFEditorWidget;
class RenderView;
struct VersionResponse;
class CommandLineParsing;
struct SelectionInfo;
class RxRunLoop;
class QMLMessageBox;

class MainWindow : public QMainWindow,
                   public Ui::mainWindow,
                   public GltfInteractorOutput,
                   public LoadInteractorOutput {
    Q_OBJECT

public:
    MainWindow(QWidget* _pParent = nullptr, const core::string _logFilename = "");
    ~MainWindow() override;

    void presentGltfOpen(gltf::Gltf*) final;
    void presentGltfReloaded(gltf::Gltf*) final;
    void presentGltfImport(gltf::Gltf*) final;
    void presentSceneOpen(const GltfSceneOpenUseCase&) final;
    static MainWindow* getMainWindow();
    void queueCapture(const CommandLineParsing::CaptureConfiguration& _configuration);
    void askToSaveBeforeLoosingState();
    void showPopup(const QString& file, const QString& title, int height, int width, int x = -1, int y = -1);
    void closePopup();

    RxRunLoop* rxRunLoop;
    GLTFEditorWidget* m_pRenderer = nullptr;
    QQmlEngine* m_qmlEngine = nullptr;

signals:
    void exited();
    void continueTaskAfterPrompt();

public slots:
    void closeMITLicense();
    void closeValidationPopup();
    void closeAboutGestaltor();
    void rendererIsReady();
    bool openFile(std::string path);

private:
    inline static MainWindow* m_instance = nullptr;

    ApplicationData* m_applicationData;

    RenderView* m_pRendererDock = nullptr;
    CustomDockWidget* m_pHierarchyDock = nullptr;
    CustomDockWidget* m_pInspectorDock = nullptr;
    CustomDockWidget* m_pResourcesDock = nullptr;
    CustomDockWidget* m_pLoggerDock = nullptr;
    CustomDockWidget* m_pViewportSettingsDock = nullptr;
    CustomDockWidget* m_pStatisticsDock = nullptr;
    HotReloadQQuickView* m_preferencesWindow = nullptr;
    HotReloadQQuickView* m_hierarchyWindow = nullptr;
    HotReloadQQuickView* m_mitLicenseWindow = nullptr;
    HotReloadQQuickView* m_aboutGestaltorWindow = nullptr;
    HotReloadQQuickView* m_popupTemplate = nullptr;
    QMLMessageBox* m_validationAfterSave = nullptr;
    void setShowWelcomeOnStartup(bool);
    bool getShowWelcomeOnStartup();
    Q_PROPERTY(bool showWelcomeOnStartup READ getShowWelcomeOnStartup WRITE setShowWelcomeOnStartup);
    bool m_showWelcomeOnStartup = true;
    HotReloadQQuickView* m_thirdPartiesWindow = nullptr;

    utils::RecentFilesHelper* m_recentFileHelper = nullptr;
    QMenu* m_recentFileMenu = nullptr;
    QMenu* m_visualizationModeMenu = nullptr;

    RenderView* initializeRenderer();
    CustomDockWidget* initializeHierarchy();
    CustomDockWidget* initializeInspector();
    CustomDockWidget* initializeResources();
    CustomDockWidget* initializeLogger();
    CustomDockWidget* initializeViewportSettings();
    void initializePreferences();
    void initializeMITLicense();
    void initializeAboutGestaltor();
    void initializeThirdParties();
    CustomDockWidget* initializeStatistics();

    void initializeLayout();
    void initializeViewActions();
    void setupMenuBarActions();

    void dropEvent(QDropEvent* _pEvent) override;
    void dragEnterEvent(QDragEnterEvent* _pEvent) override;

    void closeEvent(QCloseEvent* _pEvent) override;

    void saveSession();
    void updateWindowTitle(const QString& fileName = "");

    void updateOpenSceneUI(const gltf::Scene* scene);

    void open();
    void openByFileName(QString& _filename);
    void updateWindowFromOpenedFile(const QString& path);

    InteractorResult newSampleFile();
    void exportUSDZ();
    void newFile();
    bool save();
    void saveAs();
    void saveAsMinified();
    void restoreSession();
    void restoreDefaultGeometry();
    void loadStyleSheet();
    void loadStyleSheetFromDisk();
    void checkRequiredExtensions();

    void environmentCompatibiliyCheck();

    void openFileRecent(int _fileIndex);

    void updateUndoRedoAvailability();

    void showValidationPopup(bool askUser);

    void undo();
    void redo();

    void changeAlwaysOnTop(QWindow* window);

    QString getFileDisplayName();

    void removePropertyFromGltfHelper(std::string propertyName);
    void removeKnownExtensionFromGltfHelper(std::string propertyName);

    // used to prevent signal loop
    bool blockSelectionSignalUpdate = false;
    bool blockSelectionHeadChanged = false;

    nlohmann::json uuids;
    nlohmann::json::json_pointer createdProperty; // Pointer to created property which should be selected once the uuid is available
    bool m_dirty = false;
    bool m_closeApp = false;
};
}
