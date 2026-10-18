#pragma once

#include "CustomDockWidget.h"
#include <QVariant>

class QQmlEngine;

namespace ux3d::editor {
struct GltfGetStatistics;

class HotReloadQQuickWidget;

class StatisticsWidget : public CustomDockWidget {
    Q_OBJECT
    Q_PROPERTY(QVariantMap fileStatistics READ getFileStatistics WRITE setFileStatistics NOTIFY fileStatisticsChanged)
    Q_PROPERTY(QVariantMap gltfStatistics READ getGltfStatistics WRITE setGltfStatistics NOTIFY gltfStatisticsChanged)
    Q_PROPERTY(QVariantList sceneStatistics READ getSceneStatistics WRITE setSceneStatistics NOTIFY sceneStatisticsChanged)

public:
    explicit StatisticsWidget(QWidget*, QQmlEngine*);
    ~StatisticsWidget() override = default;

    const QVariantMap& getFileStatistics() const;
    void setFileStatistics(const QVariantMap&);
    const QVariantMap& getGltfStatistics() const;
    void setGltfStatistics(const QVariantMap&);
    const QVariantList& getSceneStatistics() const;
    void setSceneStatistics(const QVariantList&);

    Q_INVOKABLE void saveToCsvFile(QString msg);
    Q_INVOKABLE void copyToClipboard(QString msg);

signals:
    void fileStatisticsChanged();
    void gltfStatisticsChanged();
    void sceneStatisticsChanged();

private:
    HotReloadQQuickWidget* m_qmlWidget;
    QVariantMap m_fileStatistics;
    QVariantMap m_gltfStatistics;
    QVariantList m_sceneStatistics;
};
}
