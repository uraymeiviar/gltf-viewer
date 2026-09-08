#include "ApplicationData.h"
#include "Actions.h"
#include "Clipboard.h"
#include "RxRunLoop.h"
#include "core/Logger.h"
#include "model/ProjectManager.h"
#include "operations/BakeAOMap.h"
#include "reactor/Gltf.h"
#include "reactor/Reactor.h"
#include "reactor/Schema.h"
#include "widgets/MainWindow.h"
#include "widgets/QMLCard.h"

#include <rxcpp/rx.hpp>

#include <QAction>

using namespace ux3d;
using namespace ux3d::editor;

using namespace rxcpp;
using namespace rxcpp::sources;
using namespace rxcpp::operators;
using namespace rxcpp::util;

ApplicationData::ApplicationData(QObject* parent)
{
    auto& model = model::ProjectManager::getInstance()->model;
    const auto& editorModel = model::ProjectManager::getInstance()->editorModel;
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    const auto& miscWorker = model::ProjectManager::getInstance()->miscWorker;

    model.loadPlugin({ operations::bake_ao_map });

    auto onError = [&](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            error(QString::fromStdString(ex.what()));
        }
    };

    model.head.observe_on(uiWorker).subscribe([this](const nlohmann::json& head) {
        emit gltfChanged(head["gltf"].dump(2).c_str());
    },
        onError);

    model.newGltfIndex.with_latest_from(model.head).observe_on(uiWorker).subscribe([this](const auto& pair) {
        const auto& [index, head] = pair;
        emit gltfLoaded(head["gltf"].dump(2).c_str());
    },
        onError);

    model.head.observe_on(miscWorker).map([refs = reactor::gltf::schema::backwardReferences(model.m_linearizedSchema)](const nlohmann::json& head) {
                                         return reactor::gltf::gatherUnusedNodes(head["gltf"], refs);
                                     })
        .observe_on(uiWorker)
        .subscribe([this](const std::vector<nlohmann::json::json_pointer>& unusedNodes) {
            QList<QString> unusedNodesQt;
            for (const auto& unusedNode : unusedNodes) {
                unusedNodesQt.push_back(QString::fromStdString(unusedNode.to_string()));
            }
            emit unusedNodesChanged(unusedNodesQt);
        },
            onError);

    editorModel.head.observe_on(uiWorker).subscribe([this](const nlohmann::json& editorState) {
        emit editorStateChanged(editorState.dump(2).c_str());
    },
        onError);

    editorModel.selection.observe_on(uiWorker).subscribe([this](const nlohmann::json& selection) {
        emit selectionChanged(selection.dump(2).c_str());
    },
        onError);

    model.hierarchy.observe_on(uiWorker).subscribe([this](const nlohmann::json& hierarchy) {
        emit gltfHierarchyChanged(hierarchy.dump(2).c_str());
    },
        onError);

    model.resources.observe_on(uiWorker).subscribe([this](const nlohmann::json& resources) {
        emit gltfResourcesChanged(resources.dump(2).c_str());
    },
        onError);

    model.imageMetadata.observe_on(uiWorker).subscribe([this](const nlohmann::json& imageMetadata) {
        emit imageMetadataChanged(imageMetadata.dump(2).c_str());
    },
        onError);

    connect(Actions::instance().pasteDeep, &QAction::triggered, this, &ApplicationData::pasteDeepTriggered);

    connect(this, &ApplicationData::doCopy, Actions::instance().copy, &QAction::trigger);
    connect(this, &ApplicationData::doCut, Actions::instance().cut, &QAction::trigger);
    connect(this, &ApplicationData::doPaste, Actions::instance().pasteLinked, &QAction::trigger);
    connect(this, &ApplicationData::doRemove, Actions::instance().remove, &QAction::trigger);
    connect(this, &ApplicationData::dragAndDropSingle, &Clipboard::instance(), &Clipboard::dragAndDropSingle);
    connect(this, &ApplicationData::dragAndDropSelection, &Clipboard::instance(), &Clipboard::dragAndDropQJsonArray);
    connect(this, &ApplicationData::clearAndSetSelection, &Clipboard::instance(), &Clipboard::clearAndSetSelection);
    connect(this, &ApplicationData::clearAndSetSelectionPath, &Clipboard::instance(), &Clipboard::clearAndSetSelectionPath);
    connect(this, &ApplicationData::selectAllOfType, &Clipboard::instance(), &Clipboard::selectAllOfType);
    connect(this, &ApplicationData::setDefaultScene, Actions::instance().setDefaultScene, &QAction::trigger);
    connect(this, &ApplicationData::unsetDefaultScene, Actions::instance().unsetDefaultScene, &QAction::trigger);
    connect(this, &ApplicationData::openScene, Actions::instance().openScene, &QAction::trigger);
    connect(this, &ApplicationData::editImage, Actions::instance().editImageExternally, &QAction::trigger);
    connect(this, &ApplicationData::focus, Actions::instance().focus, &QAction::trigger);

    connect(this, &ApplicationData::importModel, Actions::instance().import3DModel, &QAction::trigger);
    connect(this, &ApplicationData::importMaterial, Actions::instance().importMaterial, &QAction::trigger);
    connect(this, &ApplicationData::importTexture, Actions::instance().importTexture, &QAction::trigger);
    connect(this, &ApplicationData::importImage, Actions::instance().importImage, &QAction::trigger);
    connect(this, &ApplicationData::importXmpPacket, Actions::instance().importXmpPacket, &QAction::trigger);

    connect(this, &ApplicationData::createCamera, Actions::instance().createCamera, &QAction::trigger);
    connect(this, &ApplicationData::createLight, Actions::instance().createLight, &QAction::trigger);
    connect(this, &ApplicationData::createMaterial, Actions::instance().createMaterial, &QAction::trigger);
    connect(this, &ApplicationData::createMaterialVariant, Actions::instance().createMaterialVariant, &QAction::trigger);
    connect(this, &ApplicationData::createNode, Actions::instance().createNode, &QAction::trigger);
    connect(this, &ApplicationData::createScene, Actions::instance().createScene, &QAction::trigger);
    connect(this, &ApplicationData::createAnimation, Actions::instance().createAnimation, &QAction::trigger);
    connect(this, &ApplicationData::createSampler, Actions::instance().createSampler, &QAction::trigger);
    connect(this, &ApplicationData::createTexture, Actions::instance().createTexture, &QAction::trigger);
}

void ApplicationData::execute(const QString& task)
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    model::ProjectManager::getInstance()->model.schedule(nlohmann::json::parse(task.toStdString())).error.observe_on(uiWorker).subscribe([](const std::string& e) {
        core::logInfo(e.c_str());
    });
}

void ApplicationData::executeWithCallback(const QString& task, const QJSValue& onError, const QJSValue& onCompleted)
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    const auto& result = model::ProjectManager::getInstance()->model.schedule(nlohmann::json::parse(task.toStdString()));
    result.error.observe_on(uiWorker).subscribe([onError](const std::string& e) {
        if (onError.isCallable()) {
            QJSValueList args;
            args.append(QString::fromStdString(e.c_str()));
            onError.call(args);
        }
    });
    result.success.observe_on(uiWorker).subscribe([onCompleted](const std::string& m) {
        if (onCompleted.isCallable()) {
            QJSValueList args;
            args.append(QString::fromStdString(m.c_str()));
            onCompleted.call(args);
        }
    });
}

void ApplicationData::runEditorOperation(const QString& operation)
{
    model::ProjectManager::getInstance()->editorModel.run(nlohmann::json::parse(operation.toStdString()));
}

QStringList ApplicationData::getReferenceCandidates(const QString& gltf, const QString& prefix, const QString& propertyPath)
{
    nlohmann::json::json_pointer prefixPointer;
    nlohmann::json gltfJson;
    try {
        prefixPointer = nlohmann::json::json_pointer(prefix.toStdString());
    } catch (...) {
        core::logError("Pointer invalid: %s", prefix.toStdString().c_str());
        return QStringList();
    }

    try {
        gltfJson = nlohmann::json::parse(gltf.toStdString());
    } catch (...) {
        core::logError("Gltf not parseable");
        return QStringList();
    }

    auto stringVectorPair = reactor::getReferenceCandidates(gltfJson, prefixPointer, propertyPath.toStdString());
    QStringList result;
    for (const auto& candidate : stringVectorPair) {
        result.append(QString::fromStdString(std::get<0>(candidate)));
        result.append(QString::fromStdString(std::get<1>(candidate)));
    }
    return result;
}

void ApplicationData::showPopup(const QString& file, const QString& title, int height, int width, int x, int y)
{
    auto mainWindow = MainWindow::getMainWindow();
    if (mainWindow) {
        mainWindow->showPopup(file, title, height, width, x, y);
    }
}

void ApplicationData::closePopup()
{
    auto mainWindow = MainWindow::getMainWindow();
    if (mainWindow) {
        mainWindow->closePopup();
    }
}

void ApplicationData::success(const QString& info)
{
    QMLCard::success(info);
}

void ApplicationData::info(const QString& info)
{
    QMLCard::info(info);
}

void ApplicationData::warning(const QString& warning)
{
    QMLCard::warning(warning);
}

void ApplicationData::error(const QString& error)
{
    QMLCard::error(error);
}

void ApplicationData::busyCursor()
{
    QGuiApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
}

void ApplicationData::resetCursor()
{
    QGuiApplication::restoreOverrideCursor();
}
