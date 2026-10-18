#include "Clipboard.h"
#include "Actions.h"
#include "model/ProjectManager.h"
#include "reactor/Gltf.h"
#include "widgets/QMLCard.h"

#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMimeData>

using namespace ux3d;
using namespace ux3d::editor;

Clipboard& Clipboard::instance()
{
    static Clipboard instance;
    return instance;
}

Clipboard::Clipboard()
{

    connect(Actions::instance().cut, &QAction::triggered, this, &Clipboard::cut);
    connect(Actions::instance().copy, &QAction::triggered, this, &Clipboard::copy);
    connect(Actions::instance().pasteLinked, &QAction::triggered, this, &Clipboard::paste);
    connect(Actions::instance().remove, &QAction::triggered, this, &Clipboard::remove);
    model::ProjectManager::getInstance()->editorModel.selection.subscribe([this](const nlohmann::json& json) {
        m_selection = json;
        bool isEnabled = !m_selection.empty();
        if (isEnabled) {
            for (auto& selection : m_selection) {
                if (!selection.contains("elementUUID") || selection["elementUUID"] == "") {
                    // Disable actions for headings
                    isEnabled = false;
                    break;
                }
            }
        }
        Actions::instance().remove->setEnabled(isEnabled);
        Actions::instance().copy->setEnabled(isEnabled);
        Actions::instance().cut->setEnabled(isEnabled);
        Actions::instance().rename->setEnabled(isEnabled); }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });
    model::ProjectManager::getInstance()->model.uuidPaths.subscribe([this](const nlohmann::json& json) { m_uuidPaths = json; }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });
    model::ProjectManager::getInstance()->model.headShared.subscribe([this](std::shared_ptr<const nlohmann::json> sharedPointer) { sharedGltf = sharedPointer; }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });
}

void Clipboard::dragAndDropSingle(const QString& parentUUID, const QString& localPath, const QString& uuid, const QString& targetUUID)
{
    nlohmann::json selection = nlohmann::json::array();
    selection.push_back({ { "uuid", parentUUID.toStdString() }, { "localPath", localPath.toStdString() }, { "elementUUID", uuid.toStdString() }, { "elementAbsolutPath", "" } });
    dragAndDropNlohmann(selection, targetUUID.toStdString());
}

void Clipboard::dragAndDropQJsonArray(const QJsonArray& selection, const QString& targetUUID)
{
    nlohmann::json selConverted = nlohmann::json::array();

    for (uint32_t i = 0; i < selection.size(); ++i) {
        QJsonObject item = selection.at(i).toObject();

        nlohmann::json elem;

        auto keys = item.keys();
        for (auto key : keys) {
            elem[key.toStdString()] = item[key].toString().toStdString();
        }
        selConverted.push_back(elem);
    }

    dragAndDropNlohmann(selConverted, targetUUID.toStdString());
}

void Clipboard::dragAndDropNlohmann(const nlohmann::json& selection, const std::string& targetUUID)
{
    if (m_uuidPaths.contains(targetUUID) == false || selection.size() == 0) {
        return;
    }

    // construct parameters array
    nlohmann::json paramsArray = nlohmann::json::array();
    for (auto& [key, elem] : selection.items()) {
        std::string elementUUID = elem["elementUUID"];
        std::string parentUUID = elem["uuid"];
        std::string localPath = elem["localPath"];
        std::string absolutePath = elem["elementAbsolutPath"];

        if (selection.size() == 1 || absolutePath.rfind("/nodes/", 0) == 0) {
            if (m_uuidPaths.contains(elementUUID) && m_uuidPaths.contains(parentUUID)) {
                paramsArray.push_back({ { "absolutePath", "/gltf" + m_uuidPaths[elementUUID].get<std::string>() },
                    { "referencePath", "/gltf" + m_uuidPaths[parentUUID].get<std::string>() + localPath } });
            }
        }
    }

    // construct final operation
    nlohmann::json operation = {
        { "advanced.cutAndPaste",
            { { "targetPath", "/gltf" + m_uuidPaths[targetUUID].get<std::string>() }, { "elements", paramsArray } } }
    };
    try {
        model::ProjectManager::getInstance()->model.schedule(operation);
    } catch (...) {
    }
}

void Clipboard::clearAndSetSelection(const QString& uuid, const QString& localPath)
{
    model::ProjectManager::getInstance()->editorModel.run(nlohmann::json({ { "editor.setSelection", { { { "uuid", uuid.toStdString() }, { "localPath", localPath.toStdString() } } } } }));
}

void Clipboard::clearAndSetSelectionPath(const QString& globalPath)
{
    try {
        nlohmann::json::json_pointer path(globalPath.toStdString());
        while (!path.empty() && (!sharedGltf->contains(path / "uuid") || !sharedGltf->at(path.parent_pointer()).is_array())) {
            path = path.parent_pointer();
        }
        model::ProjectManager::getInstance()->editorModel.run(nlohmann::json({ { "editor.setSelection", { { { "uuid", sharedGltf->at(path / "uuid") }, { "localPath", "" } } } } }));
    } catch (...) {
    }
}

void Clipboard::cut()
{
    if (!m_selection.empty() && m_selection.is_array()) {
        nlohmann::json references = nlohmann::json::array();
        for (auto& selectedItem : m_selection) {
            if (selectedItem["elementUUID"] != selectedItem["uuid"]) {
                references.push_back(selectedItem["referencePath"].get<std::string>());
            }
        }

        nlohmann::json operation = { { "dangerous.forEachElement", { { "elementPaths", references }, { "operation", "remove_gltf_pointer" } } } };
        model::ProjectManager::getInstance()->model.schedule(operation);
    }
    QMimeData* mimedata = new QMimeData();
    mimedata->setData("Gestaltor/clipboard", QByteArray::fromStdString(m_selection.dump()));
    QGuiApplication::clipboard()->setMimeData(mimedata);
    Actions::instance().pasteLinked->setEnabled(!m_selection.empty());
    Actions::instance().pasteDeep->setEnabled(!m_selection.empty());
}

void Clipboard::copy()
{
    QMimeData* mimedata = new QMimeData();
    mimedata->setData("Gestaltor/clipboard", QByteArray::fromStdString(m_selection.dump()));
    QGuiApplication::clipboard()->setMimeData(mimedata);
    Actions::instance().pasteLinked->setEnabled(!m_selection.empty());
    Actions::instance().pasteDeep->setEnabled(!m_selection.empty());
}

void Clipboard::remove()
{
    if (!m_selection.empty() && m_selection.is_array()) {
        nlohmann::json references = nlohmann::json::array();
        nlohmann::json actualObjects = nlohmann::json::array();

        bool isBuffer = false;
        bool isBufferView = false;
        for (auto& selectedItem : m_selection) {
            if (selectedItem.contains("elementAbsolutPath")) {
                const std::string& elementAbsolutPath = selectedItem["elementAbsolutPath"].get<std::string>();
                if (elementAbsolutPath.starts_with("/buffers")) {
                    isBuffer = true;
                    continue;
                }
                if (elementAbsolutPath.starts_with("/bufferViews")) {
                    isBufferView = true;
                    continue;
                }
            }
            if (selectedItem["elementUUID"] != selectedItem["uuid"]) {
                references.push_back(selectedItem["referencePath"]);
            } else {
                actualObjects.push_back(selectedItem["referencePath"]);
            }
        }

        if (isBuffer) {
            QMLCard::warning("Buffers can not be manually deleted.");
        }
        if (isBufferView) {
            QMLCard::warning("BufferViews can not be manually deleted.");
        }

        nlohmann::json taskDescriptions = nlohmann::json::array();
        if (!references.empty()) {
            nlohmann::json operation = { { "dangerous.forEachElement", { { "elementPaths", references }, { "operation", "remove_gltf_pointer" } } } };
            taskDescriptions.push_back(std::move(operation));
        }

        if (!actualObjects.empty()) {
            nlohmann::json operation = { { "dangerous.forEachElement", { { "elementPaths", actualObjects }, { "operation", "remove" } } } };
            taskDescriptions.push_back(std::move(operation));
        }

        if (taskDescriptions.empty()) {
            QMLCard::info("Nothing to delete.");
            return;
        }
        nlohmann::json taskDescription = { { "dangerous.combineOperations", taskDescriptions } };
        model::ProjectManager::getInstance()->model.schedule(taskDescription);

        model::ProjectManager::getInstance()->editorModel.run(nlohmann::json({ { "editor.clearSelection", nlohmann::json({}) } }));
    }
}

void Clipboard::paste()
{
    const QMimeData* mimedata = QGuiApplication::clipboard()->mimeData();
    if (m_selection.size() == 1 && mimedata != nullptr && mimedata->hasFormat("Gestaltor/clipboard")) {
        try {
            nlohmann::json data = nlohmann::json::parse(mimedata->data("Gestaltor/clipboard").toStdString());
            if (!data.empty()) {
                auto& selectedItem = m_selection.at(0);
                nlohmann::json pastePaths = nlohmann::json::array();
                for (const auto& pasteItem : data) {
                    pastePaths.push_back(m_uuidPaths.at(pasteItem["elementUUID"].get<std::string>()));
                }

                // TODO: do not use elementAbsolutePath and referencePath. Calculate them from the UUIDs
                // In rare cases these paths might be invalid
                nlohmann::json parameters = {
                    { "parent", "/gltf" + selectedItem["elementAbsolutPath"].get<std::string>() },
                    { "absoluteParent", "/gltf" + selectedItem["referencePath"].get<std::string>() }
                };
                nlohmann::json operation = { { "dangerous.forEachElement", { { "elementPaths", pastePaths }, { "operation", "advanced.paste" }, { "parameters", parameters } } } };
                model::ProjectManager::getInstance()->model.schedule(operation);
            }
        } catch (...) {
            core::debugLog(core::LogLevel::Warning, "Paste: could not parse json");
        }
    }
}

void Clipboard::selectAllOfType(const QString& type)
{
    nlohmann::json selection = nlohmann::json::array();
    try {
        nlohmann::json::json_pointer arrayPointer("/gltf" + type.toStdString());
        if (sharedGltf->contains(arrayPointer)) {
            for (auto& [key, value] : sharedGltf->at(arrayPointer).items()) {
                nlohmann::json singleSelection = nlohmann::json::object();
                singleSelection["uuid"] = value.value("uuid", "");
                singleSelection["localPath"] = "";
                selection.push_back(std::move(singleSelection));
            }
        }
    } catch (...) {
    }
    model::ProjectManager::getInstance()->editorModel.run(nlohmann::json({ { "editor.setSelection", selection } }));
}
