//
//  HotReloadQQuickView.cpp
//  Gestaltor
//
//  Created by Moritz Willy Becher on 11.03.21.
//

#include "HotReloadQQuickView.h"

#include "QMLDirFileWatcher.h"

#include <QFileSystemWatcher>
#include <QQmlContext>
#include <QQmlEngine>

#include "core/Logger.h"

using namespace ux3d::editor;

HotReloadQQuickView::HotReloadQQuickView(const QUrl& source, QWindow* parent)
    : QQuickView(source, parent)
{
    m_fileWatcher = QMLDirFileWatcher::getInstance();
    connect(this->m_fileWatcher, &QMLDirFileWatcher::filesChanged, this, &HotReloadQQuickView::reload);
}

HotReloadQQuickView::HotReloadQQuickView(QQmlEngine* engine, QWindow* parent)
    : QQuickView(engine, parent)
{
    m_fileWatcher = QMLDirFileWatcher::getInstance();
    connect(this->m_fileWatcher, &QMLDirFileWatcher::filesChanged, this, &HotReloadQQuickView::reload);
}

HotReloadQQuickView::HotReloadQQuickView(QWindow* parent)
    : QQuickView(parent)
{
    m_fileWatcher = QMLDirFileWatcher::getInstance();
    connect(this->m_fileWatcher, &QMLDirFileWatcher::filesChanged, this, &HotReloadQQuickView::reload);
}

void HotReloadQQuickView::setRootContext(const QString& context, QObject* _pValue)
{
    std::tuple<const QString, QObject*> tuple = { context, _pValue };
    m_rootContext.emplace(tuple);
    rootContext()->setContextProperty(context, _pValue);
}

void HotReloadQQuickView::reload()
{
    this->engine()->clearComponentCache();
    // set root context
    if (m_rootContext.has_value()) {
        std::tuple<const QString, QObject*> tuple = m_rootContext.value();
        this->rootContext()->setContextProperty(std::get<0>(tuple), std::get<1>(tuple));
    }
    QQuickView::setSource(this->source());
    emit reloaded();
}

void HotReloadQQuickView::closeEvent(QCloseEvent* event)
{
    if (callOnClose()) {
        event->accept();
    } else {
        event->ignore();
    }
}
