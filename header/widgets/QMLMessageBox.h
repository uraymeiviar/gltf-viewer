#pragma once

#include "HotReloadQQuickView.h"

class QQmlEngine;

namespace ux3d::editor {

class QMLMessageBox final : public HotReloadQQuickView {
    Q_OBJECT
    Q_PROPERTY(QString text READ getText WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString detailedText READ getDetailedText WRITE setDetailedText NOTIFY detailedTextChanged)
    Q_PROPERTY(bool askUser READ getAskUser WRITE setAskUser NOTIFY askUserChanged)

public:
    explicit QMLMessageBox(const QString& title, QQmlEngine* engine = nullptr);
    QString getText() const { return m_text; }
    QString getDetailedText() const { return m_detailedText; }
    bool getAskUser() const { return m_askUser; }
    void setText(const QString& text)
    {
        m_text = text;
        emit textChanged();
    }
    void setDetailedText(const QString& text)
    {
        m_detailedText = text;
        emit detailedTextChanged();
    }
    void setAskUser(bool askUser)
    {
        m_askUser = askUser;
        emit askUserChanged();
    }
    Q_INVOKABLE void wasAccepted() { emit accepted(); }
    Q_INVOKABLE void closeMessageBox()
    {
        close();
    }

signals:
    void textChanged();
    void detailedTextChanged();
    void askUserChanged();
    void accepted();

private:
    QString m_text;
    QString m_detailedText;
    bool m_askUser;
};
}
