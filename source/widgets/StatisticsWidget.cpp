#include "widgets/StatisticsWidget.h"
#include "Constants.h"
#include "GltfPropertyPointer.h"
#include "HotReloadQQuickWidget.h"
#include "helpers/QtFileSystem.h"
#include "helpers/Utils.h"
#include "model/ProjectManager.h"
#include "useCases/GltfUseCases.h"
#include "widgets/MainWindow.h"

#include "model/ProjectManager.h"
#include "reactor/DataUtils.h"
#include "reactor/Utils.h"

#include "nlohmann/json.hpp"

#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>

#include <iostream>
#include <set>

using namespace ux3d;
using namespace ux3d::editor;

void gatherPrimitiveStats(const nlohmann::json& gltf, const nlohmann::json& prim, QVariantMap& stats)
{
    qulonglong triangle_count = 0;
    qulonglong line_count = 0;
    qulonglong point_count = 0;

    qulonglong count;
    if (prim.contains("indices")) {
        int acc = prim["indices"];
        count = gltf["accessors"][acc]["count"];
    } else if (prim.contains("attributes") && prim["attributes"].contains("POSITION")) {
        int acc = prim["attributes"]["POSITION"];
        count = gltf["accessors"][acc]["count"];
    } else {
        count = 0;
    }

    switch (prim.value("mode", 4)) {
    case 0:
        point_count += count;
        break;
    case 1:
        line_count += count / 2;
        break;
    case 2:
        line_count += count == 1 ? 0 : count;
        break;
    case 3:
        line_count += count == 0 ? 0 : count - 1;
        break;
    case 4:
        triangle_count += count / 3;
        break;
    case 5:
    case 6:
        triangle_count += count < 3 ? 0 : count - 2;
        break;
    }

    stats["Triangles"] = stats["Triangles"].toULongLong() + triangle_count;
    stats["Lines"] = stats["Lines"].toULongLong() + line_count;
    stats["Points"] = stats["Points"].toULongLong() + point_count;
}

void gatherNodeStats(const nlohmann::json& gltf, int index, std::set<int>& set, QVariantMap& stats)
{
    // Don't count the same node twice
    if (set.contains(index)) {
        return;
    }
    set.insert(index);

    const nlohmann::json& node = gltf["nodes"][index];

    stats["Nodes"] = stats["Nodes"].toULongLong() + 1;

    if (node.contains("camera")) {
        stats["Cameras"] = stats["Cameras"].toULongLong() + 1;
    }
    if (node.contains("skin")) {
        stats["Skins"] = stats["Skins"].toULongLong() + 1;
    }
    if (node.contains("mesh")) {
        stats["Meshes"] = stats["Meshes"].toULongLong() + 1;
        int mesh = node["mesh"];
        for (const auto& prim : gltf["meshes"][mesh]["primitives"]) {
            gatherPrimitiveStats(gltf, prim, stats);
        }
    }
    if (node.contains("extensions") && node["extensions"].contains("KHR_lights_punctual")) {
        stats["Lights"] = stats["Lights"].toULongLong() + 1;
    }

    if (node.contains("children")) {
        for (const auto& child : node["children"]) {
            gatherNodeStats(gltf, child, set, stats);
        }
    }
}

StatisticsWidget::StatisticsWidget(QWidget* parent, QQmlEngine* engine)
    : CustomDockWidget(parent)
{
    m_qmlWidget = new HotReloadQQuickWidget(engine, this);
    m_qmlWidget->setRootContext("statistics", this);
    m_qmlWidget->setSource(QUrl(QString(UX3D_QML_ROOT_DIR_URL) + "/statistics.qml"));
    m_qmlWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_qmlWidget->setMinimumWidth(300);
    this->setWidget(m_qmlWidget);

    setObjectName("StatisticsDockWidget");

    auto& model = model::ProjectManager::getInstance()->model;
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;

    model.headRaw.observe_on(uiWorker).subscribe([this](const nlohmann::json& head) {
        if (!head.contains("gltf")) {
            return;
        }
        const auto& gltf = head.at("gltf");

        auto& datastorage = model::ProjectManager::getInstance()->model.m_dataStorage;

        QVariantMap map;

        {
            std::string gltfString = gltf.dump(1, '\t', false, nlohmann::json::error_handler_t::ignore);
            uint64_t fileSize = gltfString.length();
            fileSize += reactor::data::getDataSize(head, datastorage);
            map["glTF file size"] = QVariant { utils::convertBytesToString(fileSize) };
        }

        if (gltf.contains("images")) {
            uint64_t imageSize = 0;

            for (const auto& image : gltf["images"]) {
                if (image.contains("bufferView")) {
                    int bufferView = image["bufferView"];
                    imageSize += gltf["bufferViews"][bufferView]["byteLength"].get<uint64_t>();
                }
                if (image.contains("uri")) {
                    std::string uri = image["uri"];
                    if (head.at("embedded").contains(uri)) {
                        auto id = head.at("embedded").at(uri).get<int>();
                        imageSize += datastorage.at(id).size();
                    }
                }
            }

            map["Total image size"] = QVariant { utils::convertBytesToString(imageSize) };
        } else {
            map["Total image size"] = QVariant { utils::convertBytesToString(0) };
        }

        setFileStatistics(map); }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });

    model.headRaw.observe_on(uiWorker).subscribe([this](const nlohmann::json& head) {
        if (!head.contains("gltf")) {
            return;
        }
        const auto& gltf = head["gltf"];

        QVariantMap map;
        if (gltf.contains("cameras")) {
            map["Cameras"] = (qulonglong)gltf["cameras"].size();
        } else {
            map["Cameras"] = 0;
        }
        if (gltf.contains("nodes")) {
            map["Nodes"] = (qulonglong)gltf["nodes"].size();
        } else {
            map["Nodes"] = 0;
        }
        if (gltf.contains("scenes")) {
            map["Scenes"] = (qulonglong)gltf["scenes"].size();
        } else {
            map["Scenes"] = 0;
        }
        if (gltf.contains("textures")) {
            map["Textures"] = (qulonglong)gltf["textures"].size();
        } else {
            map["Textures"] = 0;
        }
        if (gltf.contains("images")) {
            map["Images"] = (qulonglong)gltf["images"].size();
        } else {
            map["Images"] = 0;
        }
        if (gltf.contains("samplers")) {
            map["Samplers"] = (qulonglong)gltf["samplers"].size();
        } else {
            map["Samplers"] = 0;
        }
        if (gltf.contains("animations")) {
            map["Animations"] = (qulonglong)gltf["animations"].size();
        } else {
            map["Animations"] = 0;
        }

        map["Meshes"] = 0;
        map["Triangles"] = 0;
        map["Lines"] = 0;
        map["Points"] = 0;
        if (gltf.contains("meshes")) {
            map["Meshes"] = (qulonglong)gltf["meshes"].size();
            for (const auto& mesh : gltf.at("meshes")) {
                if (mesh.contains("primitives")) {
                    for (const auto& prim : mesh.at("primitives")) {
                        gatherPrimitiveStats(gltf, prim, map);
                    }
                }
            }
        }

        if (gltf.contains("skins")) {
            map["Skins"] = (qulonglong)gltf["skins"].size();
        } else {
            map["Skins"] = 0;
        }
        if (gltf.contains("accessors")) {
            map["Accessors"] = (qulonglong)gltf["accessors"].size();
        } else {
            map["Accessors"] = 0;
        }
        if (gltf.contains("extensions") && gltf["extensions"].contains("KHR_lights_punctual")) {
            map["Lights"] = (qulonglong)gltf["extensions"]["KHR_lights_punctual"]["lights"].size();
        } else {
            map["Lights"] = 0;
        }

        setGltfStatistics(map); }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });

    model.headRaw.observe_on(uiWorker).subscribe([this](const nlohmann::json& head) {
        QVariantList stats;
        if (!head.contains("/gltf/scenes"_json_pointer)) {
            setSceneStatistics(stats);
            return;
        }
        const auto& gltf = head["gltf"];

        for (int i = 0; i < gltf["scenes"].size(); i++) {
            const auto& scene = gltf["scenes"][i];

            QVariantMap sceneMap;
            if (scene.contains("name")) {
                sceneMap["key"] = QString::fromStdString(scene["name"]);
            } else {
                sceneMap["key"] = "Unnamed Scene " + QString::number(i);
            }

            QVariantMap map;
            map["Triangles"] = 0;
            map["Lines"] = 0;
            map["Points"] = 0;
            map["Nodes"] = 0;
            map["Cameras"] = 0;
            map["Lights"] = 0;
            map["Meshes"] = 0;
            map["Skins"] = 0;

            std::set<int> set;
            if (scene.contains("nodes")) {
                for (const auto& node : scene.at("nodes")) {
                    gatherNodeStats(gltf, node, set, map);
                }
            }

            sceneMap["values"] = map;
            stats.append(sceneMap);
        }

        setSceneStatistics(stats); }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });
}

const QVariantMap& StatisticsWidget::getFileStatistics() const
{
    return m_fileStatistics;
}

void StatisticsWidget::setFileStatistics(const QVariantMap& statistics)
{
    if (m_fileStatistics == statistics) {
        return;
    }
    m_fileStatistics = statistics;
    emit fileStatisticsChanged();
}

const QVariantMap& StatisticsWidget::getGltfStatistics() const
{
    return m_gltfStatistics;
}

void StatisticsWidget::setGltfStatistics(const QVariantMap& statistics)
{
    if (m_gltfStatistics == statistics) {
        return;
    }
    m_gltfStatistics = statistics;
    emit gltfStatisticsChanged();
}

const QVariantList& StatisticsWidget::getSceneStatistics() const
{
    return m_sceneStatistics;
}

void StatisticsWidget::setSceneStatistics(const QVariantList& statistics)
{
    if (m_sceneStatistics == statistics) {
        return;
    }
    m_sceneStatistics = statistics;
    emit sceneStatisticsChanged();
}

void StatisticsWidget::saveToCsvFile(QString msg)
{
    QString path = QString::fromStdString(model::ProjectManager::getInstance()->getGltfPath());

    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + constants::unnamedGltfName;
    }

    const filesystem::File file = QtFileSystem::instance().toFile(path.toStdString());
    std::string pathWithoutExtension = (file.getParent() / file.getStem()).toString() + "Statistics";

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), QString::fromStdString(pathWithoutExtension), tr("*.csv"));

    QtFileSystem::instance().toFile(fileName.toStdString()).save(msg.toStdString());
}

void StatisticsWidget::copyToClipboard(QString msg)
{
    auto clipboard = QGuiApplication::clipboard();
    clipboard->setText(msg);
}
