//
//  HotReloadQQuickWidget.cpp
//  Gestaltor
//
//  Created by Moritz Willy Becher on 11.03.21.
//

#include "HotReloadQQuickWidget.h"

#include "QMLDirFileWatcher.h"

#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QQmlContext>
#include <QQmlEngine>
#include <QUrl>

#include "core/Logger.h"

using namespace ux3d::editor;

HotReloadQQuickWidget::HotReloadQQuickWidget(const QUrl& source, QWidget* parent)
    : QQuickWidget(source, parent)
{
    m_fileWatcher = QMLDirFileWatcher::getInstance();
    connect(this->m_fileWatcher, &QMLDirFileWatcher::filesChanged, this, &HotReloadQQuickWidget::reload);
}

HotReloadQQuickWidget::HotReloadQQuickWidget(QQmlEngine* engine, QWidget* parent)
    : QQuickWidget(engine, parent)
{
    m_fileWatcher = QMLDirFileWatcher::getInstance();
    connect(this->m_fileWatcher, &QMLDirFileWatcher::filesChanged, this, &HotReloadQQuickWidget::reload);
}

HotReloadQQuickWidget::HotReloadQQuickWidget(QWidget* parent)
    : QQuickWidget(parent)
{
    m_fileWatcher = QMLDirFileWatcher::getInstance();
    connect(this->m_fileWatcher, &QMLDirFileWatcher::filesChanged, this, &HotReloadQQuickWidget::reload);
}

void HotReloadQQuickWidget::setRootContext(const QString& context, QObject* _pValue)
{
    std::tuple<const QString, QObject*> tuple = { context, _pValue };
    m_rootContext.emplace(tuple);
    rootContext()->setContextProperty(context, _pValue);
}

void HotReloadQQuickWidget::reload()
{
    if (this->source().toString().isEmpty()) {
        // nothing to reload
        return;
    }

    this->engine()->clearComponentCache();
    // set root context
    if (m_rootContext.has_value()) {
        std::tuple<const QString, QObject*> tuple = m_rootContext.value();
        this->rootContext()->setContextProperty(std::get<0>(tuple), std::get<1>(tuple));
    }
    QQuickWidget::setSource(this->source());
    emit reloaded();
}
