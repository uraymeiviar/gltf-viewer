// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <QObject>

#include <nlohmann/json.hpp>

namespace ux3d::editor {
class Clipboard : public QObject {
    Q_OBJECT
public:
    static Clipboard& instance();
    Clipboard(Clipboard const&) = delete;
    void operator=(Clipboard const&) = delete;

    void dragAndDropSingle(const QString& parentUUID, const QString& localPath, const QString& uuid, const QString& targetUUID);
    void dragAndDropQJsonArray(const QJsonArray& selection, const QString& targetUUID);
    void clearAndSetSelection(const QString& uuid, const QString& localPath);
    void clearAndSetSelectionPath(const QString& globalPath);
    void selectAllOfType(const QString& type);

private:
    Clipboard();
    virtual ~Clipboard() = default;
    void copy();
    void cut();
    void paste();
    void remove();
    void dragAndDropNlohmann(const nlohmann::json& selection, const std::string& targetUUID);

    nlohmann::json m_selection;
    nlohmann::json m_uuidPaths;
    std::shared_ptr<const nlohmann::json> sharedGltf;
};
}
