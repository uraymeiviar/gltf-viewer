
#pragma once

#include <QQuickWidget>

#include <optional>
#include <tuple>

class QFileSystemWatcher;

namespace ux3d::editor {
class QMLDirFileWatcher;

class HotReloadQQuickWidget : public QQuickWidget {
    Q_OBJECT

public:
    HotReloadQQuickWidget(const QUrl& source, QWidget* parent = nullptr);
    HotReloadQQuickWidget(QQmlEngine* engine, QWidget* parent);
    HotReloadQQuickWidget(QWidget* parent = nullptr);

    void setRootContext(const QString& _context, QObject* _pValue);
    void observeFile(const QUrl& _file);

signals:
    void reloaded();

public slots:
    void reload();

private:
    std::optional<std::tuple<const QString, QObject*>> m_rootContext;

    QMLDirFileWatcher* m_fileWatcher;
};

}
