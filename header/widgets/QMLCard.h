#pragma once

#include "HotReloadQQuickView.h"
#include "core/DispatchQueue.h"
#include <queue>
#include <tuple>

class QQmlEngine;

namespace ux3d::editor {

class QMLCard final : public HotReloadQQuickView {
    Q_OBJECT
    Q_PROPERTY(QString text READ getText WRITE setText NOTIFY textChanged)
    Q_PROPERTY(int severity READ getSeverity WRITE setSeverity NOTIFY severityChanged)

public:
    static void info(const QString& info);
    static void warning(const QString& warning);
    static void error(const QString& error);
    static void success(const QString& success);
    static void message(int severity, const QString& message);
    static void resolvePromiseToCard(const model::DispatchQueue::ResultPipes& pipe, const std::string& successMessage = "", bool errorAsInfo = false, bool logSuccess = true, bool logError = true);

    QString getText() const { return m_text; }
    int getSeverity() const { return m_severity; }
    void setText(const QString& text)
    {
        m_text = text;
        emit textChanged();
    }
    void setSeverity(int severity)
    {
        m_severity = severity;
        emit severityChanged();
    }
    Q_INVOKABLE void closeCard();

signals:
    void textChanged();
    void severityChanged();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    explicit QMLCard(int severity, const QString& message, QQmlEngine* engine = nullptr);
    QString m_text;
    int m_severity;
    static std::queue<std::tuple<int, QString>> messageQueue;
    static QMLCard* currentCard;
};
}
