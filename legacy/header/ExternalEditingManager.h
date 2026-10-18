#pragma once

#include "interfaces/GltfInteractorOutput.h"
#include "interfaces/LoadInteractorOutput.h"

#include <QObject>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class QFileSystemWatcher;
class QTimer;

namespace ux3d::editor {
class ExternalEditingManager : public QObject, public LoadInteractorOutput, public GltfInteractorOutput {
public:
    static ExternalEditingManager* getInstance();
    ~ExternalEditingManager();
    ExternalEditingManager(ExternalEditingManager const&) = delete;
    void operator=(ExternalEditingManager const&) = delete;

    void edit(std::string image, const nlohmann::json& state);
    void unregister();

    void presentGltfOpen(gltf::Gltf*) final;

private:
    QFileSystemWatcher* m_fileSystemWatcher = nullptr;

    ExternalEditingManager();
};
}
