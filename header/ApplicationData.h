// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <QJSValue>
#include <QJsonObject>
#include <QObject>

#include <list>
#include <nlohmann/json.hpp>

namespace ux3d::editor {

/// ApplicationData is a QObject that allows the QML side access to core application data streams. Note that
/// this class doesn't contain the actual application data state, but only makes it available to QML
class ApplicationData : public QObject {
    Q_OBJECT

public:
    ApplicationData(QObject* parent = nullptr);

    Q_INVOKABLE QStringList getReferenceCandidates(const QString& gltf, const QString& prefix, const QString& propertyPath);
    Q_INVOKABLE void showPopup(const QString& file, const QString& title, int height, int width, int x = -1, int y = -1);
    Q_INVOKABLE void closePopup();
    Q_INVOKABLE void success(const QString& info);
    Q_INVOKABLE void info(const QString& info);
    Q_INVOKABLE void warning(const QString& warning);
    Q_INVOKABLE void error(const QString& error);

    Q_INVOKABLE void busyCursor();
    Q_INVOKABLE void resetCursor();

signals:
    void gltfChanged(const QString& gltf);
    void gltfHierarchyChanged(const QString& hierarchy);
    void gltfResourcesChanged(const QString& resources);
    void editorStateChanged(const QString& editorState);
    void selectionChanged(const QString& selection);
    void unusedNodesChanged(const QList<QString>& unusedNodes);
    void gltfLoaded(const QString& gltf);
    void imageMetadataChanged(const QString& metadata);

    // UI Actions
    void removeTriggered();
    void copyTriggered();
    void pasteLinkedTriggered();
    void pasteDeepTriggered();
    void cloneLinkedTriggered();
    void cloneDeepTriggered();

    Q_INVOKABLE void doCut();
    Q_INVOKABLE void doCopy();
    Q_INVOKABLE void doPaste();
    Q_INVOKABLE void doRemove();
    Q_INVOKABLE void dragAndDropSingle(const QString& parentUUID, const QString& localPath, const QString& uuid, const QString& targetUUID);
    Q_INVOKABLE void dragAndDropSelection(const QJsonArray& selection, const QString& targetUUID);
    Q_INVOKABLE void setDefaultScene();
    Q_INVOKABLE void unsetDefaultScene();
    Q_INVOKABLE void openScene();
    Q_INVOKABLE void importModel();
    Q_INVOKABLE void importMaterial();
    Q_INVOKABLE void importXmpPacket();
    Q_INVOKABLE void importImage();
    Q_INVOKABLE void importTexture();
    Q_INVOKABLE void editImage();
    Q_INVOKABLE void focus();
    Q_INVOKABLE void clearAndSetSelection(const QString& uuid, const QString& localPath);
    Q_INVOKABLE void clearAndSetSelectionPath(const QString& globalPath);
    Q_INVOKABLE void selectAllOfType(const QString& type);

    Q_INVOKABLE void createNode();
    Q_INVOKABLE void createScene();
    Q_INVOKABLE void createCamera();
    Q_INVOKABLE void createLight();
    Q_INVOKABLE void createMaterial();
    Q_INVOKABLE void createAnimation();
    Q_INVOKABLE void createTexture();
    Q_INVOKABLE void createSampler();
    Q_INVOKABLE void createMesh();
    Q_INVOKABLE void createMaterialVariant();

public slots:
    void execute(const QString& task);
    void executeWithCallback(const QString& task, const QJSValue& onError, const QJSValue& onCompleted);
    void runEditorOperation(const QString& operation);
};
}
