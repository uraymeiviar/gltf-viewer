
#pragma once

#include <QQuickView>

#include <optional>
#include <tuple>

namespace ux3d::editor {
class QMLDirFileWatcher;

class HotReloadQQuickView : public QQuickView {
    Q_OBJECT

public:
    HotReloadQQuickView(const QUrl& source, QWindow* parent = nullptr);
    HotReloadQQuickView(QQmlEngine* engine, QWindow* parent = nullptr);
    HotReloadQQuickView(QWindow* parent = nullptr);

    void setRootContext(const QString& context, QObject* _pValue);
    std::function<bool()> callOnClose = []() { return true; };

signals:
    void reloaded();

public slots:
    void reload();

private:
    QMLDirFileWatcher* m_fileWatcher;
    std::optional<std::tuple<const QString, QObject*>> m_rootContext;
    void closeEvent(QCloseEvent* event) override;
};

}
