#include "ExternalEditingManager.h"

#include "Actions.h"
#include "Preferences.h"
#include "core/Platform.h"
#include "helpers/QtFileSystem.h"
#include "interactors/GltfInteractor.h"
#include "interactors/LoadInteractor.h"
#include "interactors/SaveInteractor.h"
#include "useCases/GltfUseCases.h"

#include <QAction>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QtCore/qprocess.h>
#include <QtCore/qstandardpaths.h>

#include "reactor/Utils.h"

using namespace ux3d;
using namespace ux3d::editor;

ExternalEditingManager::ExternalEditingManager()
{
    m_fileSystemWatcher = new QFileSystemWatcher(this);

    GltfInteractor::getInstance()->registerOutput(this);
    LoadInteractor::getInstance()->registerOutput(this);
}

ExternalEditingManager::~ExternalEditingManager()
{
    GltfInteractor::getInstance()->unregisterOutput(this);
    LoadInteractor::getInstance()->unregisterOutput(this);

    for (const QString& path : m_fileSystemWatcher->files()) {
        // remove all temp files we created
        QtFileSystem::instance().remove(filesystem::Path(QtFileSystem::instance(), path.toStdString()));
    }
}

void ExternalEditingManager::edit(std::string image, const nlohmann::json& state)
{
    const QString programPath = QDir::toNativeSeparators(Preferences::getInstance()->getImageEditingToolPath());
    if (programPath.isEmpty() || programPath == ".") {
        // no program has been set yet, the user needs to choose one
        QMessageBox messageBox = QMessageBox();
        messageBox.setWindowTitle("Missing Path");
        messageBox.setText("Please set a path to an image editing tool in the Preferences");
        QPushButton* preferencesButton = new QPushButton("Open Preferences", &messageBox);
        connect(preferencesButton, &QPushButton::clicked, Actions::instance().preferences, &QAction::trigger);
        messageBox.addButton(preferencesButton, QMessageBox::ButtonRole::ActionRole);
        messageBox.addButton(QMessageBox::StandardButton::Ok);
        messageBox.exec();
        return;
    }

    // run external tool
    auto run_external_tool_f = [this, programPath](QString osNativeImagePath, std::string gltfImagePath) {
        QProcess* editProcess = new QProcess(this);
        connect(editProcess, &QProcess::readyRead, [editProcess] {
            core::debugLog(core::LogLevel::Info, editProcess->readAllStandardOutput().toStdString().c_str());
        });
        editProcess->setProcessChannelMode(QProcess::MergedChannels);

#ifdef UX3D_IS_APPLE
        editProcess->setProgram("open");
        editProcess->setArguments({ osNativeImagePath, "-a", programPath, "--wait-apps" }); // path of the image to open
#else
        editProcess->setProgram(programPath);
        editProcess->setArguments({ osNativeImagePath }); // path of the image to open
#endif

        editProcess->start();

        // watch the file in temp dir for changes
        if (!m_fileSystemWatcher->files().contains(osNativeImagePath)) {
            m_fileSystemWatcher->addPath(osNativeImagePath);
        }

#ifdef UX3D_IS_LINUX
        connect(editProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, osNativeImagePath, gltfImagePath](int, QProcess::ExitStatus) {
            auto params = nlohmann::json::object();
            params["filepath"] = osNativeImagePath.toStdString();
            params["path"] = gltfImagePath;
            auto taskDescription = nlohmann::json({ { "reload_file", params } });
            const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
            model::ProjectManager::getInstance()->model.schedule(taskDescription).error.observe_on(uiWorker).subscribe([](const std::string& e) {
            });
        });
#else
        connect(m_fileSystemWatcher, &QFileSystemWatcher::fileChanged, this, [this, osNativeImagePath, gltfImagePath](const QString filePath) {
            auto params = nlohmann::json::object();
            params["filepath"] = osNativeImagePath.toStdString();
            params["path"] = gltfImagePath;
            auto taskDescription = nlohmann::json({ { "reload_file", params } });
            const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
            model::ProjectManager::getInstance()->model.schedule(taskDescription).error.observe_on(uiWorker).subscribe([](const std::string& e) {}, [](std::exception_ptr ep) {
                try {
                    std::rethrow_exception(ep);
                } catch (const std::exception& ex) {
                    core::logError("%s", ex.what());
                } });
        });
#endif
    };

    // save image to temp directory
    const auto imageProperty = state.at(nlohmann::json::json_pointer(image));
    auto dataIDPath = reactor::utils::getDataIDPath(imageProperty, state);
    uint64_t dataID = state.at(dataIDPath);

    std::string tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation).toStdString();
    std::string filePath = tempPath + "/" + std::to_string(dataID) + ".png";
    QString osNativeImagePath = QDir::toNativeSeparators(QString::fromStdString(filePath));

    auto params = nlohmann::json::object();
    params["filepath"] = filePath;
    params["path"] = image;
    auto taskDescription = nlohmann::json({ { "export_image", params } });

    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    model::ProjectManager::getInstance()->model.schedule(taskDescription).success.observe_on(uiWorker).subscribe([run_external_tool_f, osNativeImagePath, image](const std::string& r) { run_external_tool_f(osNativeImagePath, image); }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });
}

void ExternalEditingManager::unregister()
{
    m_fileSystemWatcher->disconnect();
    const QStringList paths = m_fileSystemWatcher->files();
    for (const QString& path : paths) {
        QtFileSystem::instance().remove(filesystem::Path(QtFileSystem::instance(), path.toStdString()));
    }
    m_fileSystemWatcher->removePaths(paths);

    if (paths.size() > 0) {
        core::logInfo("Image editing has been canceled");
    }
}

void ExternalEditingManager::presentGltfOpen(gltf::Gltf*)
{
    m_fileSystemWatcher->disconnect();
    const QStringList paths = m_fileSystemWatcher->files();
    for (const QString& path : paths) {
        QtFileSystem::instance().remove(filesystem::Path(QtFileSystem::instance(), path.toStdString()));
    }
    m_fileSystemWatcher->removePaths(paths);
}

ExternalEditingManager* ExternalEditingManager::getInstance()
{
    static ExternalEditingManager instance;
    return &instance;
}
