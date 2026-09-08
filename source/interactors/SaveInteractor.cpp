#include "interactors/SaveInteractor.h"
#include "Constants.h"
#include "Preferences.h"
#include "Version.h"
#include "core/Logger.h"
#include "core/Platform.h"
#include "gltf/ExportOptions.h"
#include "gltf/GltfParsing.h"
#include "gltf/Image.h"
#include "helpers/FileFilters.h"
#include "helpers/GltfObjectSelector.h"
#include "helpers/MemoryStreamFileSystem.h"
#include "helpers/QtFileSystem.h"
#include "model/ProjectManager.h"
#include "widgets/QMLCard.h"

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QObject>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextCodec>

using namespace ux3d;
using namespace ux3d::editor;

SaveInteractor* SaveInteractor::getInstance()
{
    static SaveInteractor instance;
    return &instance;
}

InteractorResult SaveInteractor::exportToUSD()
{
    std::string path = model::ProjectManager::getInstance()->getGltfPath();
    if (path.empty()) {
        path = (QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + constants::unnamedGltfName).toStdString();
    }

    // remove file ending
    // the file ending can contain gltf, glb, or usdz.
    // it easier to just always remove it than to check if its the correct one
    filesystem::File tmpFile = QtFileSystem::instance().toFile(path);
    std::string pathWithoutExtension = (tmpFile.getParent() / tmpFile.getStem()).toString();
    path = pathWithoutExtension + ".usdz";

    const QString filter = "usdz (*.usdz)";

    std::string suffix;
    tie(path, suffix) = askUserForPathAndType(path, filter);

    if (path.empty()) {
        return InteractorResult::Fail;
    }

    filesystem::File file = QtFileSystem::instance().toFile(path);
    auto taskDescription = nlohmann::json({ { "export_usdz", nlohmann::json::object() } });
    taskDescription["export_usdz"]["filepath"] = file.toString();
    taskDescription["export_usdz"]["overwrite"] = true;

    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(taskDescription), "USDZ export successful");

    return InteractorResult::Success;
}

InteractorResult SaveInteractor::saveProject(AskForLocation _behavior, bool _saveMinified)
{
    std::string path = model::ProjectManager::getInstance()->getGltfPath();
    SaveFileFilter type = getFileTypeFromPath(path);

    bool allowResourceOverwriting = true;

    if (_behavior == AskForLocation::Always || path.empty()) {

        QString filter;
        switch (type) {
        case SaveFileFilter::Gltf:
            filter = saveFileFiltersToString({ SaveFileFilter::Gltf, SaveFileFilter::Glb });
            break;
        default:
        case SaveFileFilter::Glb:
            filter = saveFileFiltersToString({ SaveFileFilter::Glb, SaveFileFilter::Gltf });
            break;
        }

        // addapt path and filters to account for minified versions
        if (_saveMinified) {
            QString qsPath = QString::fromStdString(path);
            if (qsPath.isEmpty()) {
                qsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + constants::unnamedGltfName;
            }

            filesystem::File file = QtFileSystem::instance().toFile(qsPath.toStdString());
            std::string pathWithoutExtension = (file.getParent() / file.getStem()).toString();
            path = pathWithoutExtension.append(".gltf");
            filter = saveFileFiltersToString({ SaveFileFilter::Gltf });
        }

        std::string suffix;
        tie(path, suffix) = askUserForPathAndType(path, filter);

        if (path.empty()) {
            return InteractorResult::Fail;
        }

        if (false == checkFileExtension(path, suffix)) {
            return InteractorResult::Fail;
        }

        type = saveFileFilterFromString(QString::fromStdString(suffix));
        allowResourceOverwriting = false;
    }

    if (path.empty()) {
        return InteractorResult::Fail;
    }

    filesystem::File file = QtFileSystem::instance().toFile(path);
    return saveToFile(file, type, GltfObjectSelectionScheme::All, allowResourceOverwriting, _saveMinified);
}

InteractorResult SaveInteractor::saveToMemoryStream(core::Vector<uint8_t>& _output, SaveFileFilter _type,
    GltfObjectSelectionScheme _selectionScheme)
{
    MemoryStreamFileSystem memoryStream(_output);

    gltf::Gltf& gltf = model::ProjectManager::getInstance()->getGltf();
    gltf::ExportOptions options {};
    options.overwriteResourceFiles = true;
    options.writeToMemory = true;

    switch (_type) {
    case SaveFileFilter::Gltf:
        options.fileContainer = gltf::FileContainer::Gltf;
        options.conversionUsage = gltf::ConversionUsage::ValidGltf;
        break;
    default:
        // nothing is to be done with the storage config because glb is handled independently
        options.fileContainer = gltf::FileContainer::Glb;
        options.conversionUsage = gltf::ConversionUsage::ValidGltf;
        break;
    }

    if (_selectionScheme == GltfObjectSelectionScheme::All) {
        options.exportCandidates.clear(); // If we store all elements, we do not have to collect candidates
    } else {
        GltfObjectSelector candidatesCollector(gltf, _selectionScheme);
        candidatesCollector.gather(options.exportCandidates);
    }
    core::string generator = constants::appName;
    generator += " ";
    generator += editor::Version::get();

    options.generatorNameOverride = generator;

    gltf::GltfToFileResult result = gltf::GltfParsing::toFile(gltf, memoryStream, options);

    if (core::Failed(result)) {
        return InteractorResult::Fail;
    }

    return InteractorResult::Success;
}

std::string SaveInteractor::saveImageToTemp(gltf::Image* image)
{
    std::string tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation).toStdString();

    const core::Vector<uint8_t>* encodedImage = image->getEncodedBytes();
    std::string imageUri = image->getUri();
    if (imageUri == "") {
        char buffer[100];
        sprintf(buffer, "%d", image->m_ID);
        imageUri = std::string(buffer);
        imageUri += imageformats::getFileExtension(image->getMimeType());
    }

    tempPath += "/" + imageUri;

    filesystem::File file = QtFileSystem::instance().toFile(tempPath);
    const filesystem::Dir parentDir = file.getParent();

    if (!parentDir.exists()) {
        parentDir.create();
    }
    auto result = file.save(*encodedImage);

    return tempPath;
}

InteractorResult SaveInteractor::saveToFile(const filesystem::File& _file, SaveFileFilter _type,
    GltfObjectSelectionScheme _selectionScheme, bool _allowResourceOverwriting, bool _saveMinified)
{
    // not yet implemented
    // core::string generator = constants::appName;
    // generator += " ";
    // generator += editor::Version::get();

    // Preferences::getInstance()->getStoreAutogeneratedNames();
    // Preferences::getInstance()->getReplaceOriginalBufferUri();

    // if (!_allowResourceOverwriting) {
    //     overwriteResources = askForOverwritingPermissions();
    // }

    nlohmann::json params = {};
    params["filepath"] = _file.toString();
    params["overwrite"] = true;
    params["cleanupData"] = true;

    // params["minified"] = _saveMinified; // not yet implemented
    QMLCard::resolvePromiseToCard(model::ProjectManager::getInstance()->model.schedule(nlohmann::json({ { "save_file", params } })), "File saved succesfully");

    model::ProjectManager::getInstance()->setGltfPath(_file.toString());

    return InteractorResult::Success;
}

bool SaveInteractor::askForOverwritingPermissions()
{
    if (Preferences::getInstance()->getOverwriteResourceFilesWithoutAsking()) {
        return true;
    }

    QMessageBox messageBox;
    messageBox.setWindowTitle("Resource Files Already Exist");
    messageBox.setText("Resource files already exist. \nDo you want to continue and overwrite existing resource files?");
    messageBox.setIcon(QMessageBox::Icon::Warning);
    messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    messageBox.setDefaultButton(QMessageBox::Yes);
    messageBox.addButton("Overwrite", QMessageBox::YesRole);

    int result = messageBox.exec();

    switch (result) {
    case QMessageBox::Cancel:
        return false;

    case QMessageBox::Yes:
        return true;
    }

    return false;
}

std::pair<std::string, std::string> SaveInteractor::askUserForPathAndType(const std::string& _initialPath, const QString& _filter)
{
    QString path = QString::fromStdString(_initialPath);

    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + constants::unnamedGltfName;
    }

    filesystem::File file = QtFileSystem::instance().toFile(path.toStdString());
    std::string pathWithoutExtension = (file.getParent() / file.getStem()).toString();
    std::string extension = file.getExtension();

    QFileDialog dialog(nullptr, "Save as...", QString::fromStdString(pathWithoutExtension), _filter);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setOption(QFileDialog::DontConfirmOverwrite, false);

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().empty()) {
        return { /* none */ };
    }

    QString selectedFile = dialog.selectedFiles().front();
    QString selectedType = dialog.selectedNameFilter();

#ifdef UX3D_IS_APPLE
    filesystem::Path targetDirectory = QtFileSystem::instance().toFile(selectedFile.toStdString()).getParent();
    filesystem::IFileSystem::sandboxRequestAccess(targetDirectory);
#endif

    return { selectedFile.toStdString(), selectedType.toStdString() };
}

bool SaveInteractor::checkFileExtension(const std::string& _path, const std::string& _type)
{
    auto info = QFileInfo(QString::fromStdString(_path)).suffix().toStdString();

    if (info != "glb" && info != "gltf") {
        QMessageBox msgBox;
        msgBox.setText("No filename extension specified. This can lead to problems when loading the asset.");
        msgBox.setInformativeText("Do you still want to save without extension?");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Cancel);
        auto acceptButton = msgBox.addButton("Save without extension", QMessageBox::ButtonRole::AcceptRole);
        msgBox.setDefaultButton(acceptButton);
        msgBox.exec();
        return msgBox.clickedButton() == acceptButton;
    }

    return true;
}

const SaveFileFilter SaveInteractor::getFileTypeFromPath(const std::string& _path)
{
    if (_path == "") {
        return SaveFileFilter::None;
    }
    size_t pointPos = _path.find_last_of(".");
    if (_path.compare(pointPos, _path.size(), ".gltf") == 0) {
        return SaveFileFilter::Gltf;
    } else if (_path.compare(pointPos, _path.size(), ".glb") == 0) {
        return SaveFileFilter::Glb;
    }

    return SaveFileFilter::None;
}
