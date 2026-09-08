#include "widgets/QMLCard.h"
#include "core/Logger.h"
#include "model/ProjectManager.h"
#include "widgets/MainWindow.h"

using namespace ux3d::editor;

QMLCard* QMLCard::currentCard = nullptr;
std::queue<std::tuple<int, QString>> QMLCard::messageQueue = {};

QMLCard::QMLCard(int severity, const QString& message, QQmlEngine* _pEngine)
    : HotReloadQQuickView(_pEngine)
    , m_severity(severity)
    , m_text(message)
{
    setFlags(Qt::ToolTip | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);

    setColor(QColor(Qt::transparent));

    setRootContext("card", this);
    setSource(QUrl(QString(UX3D_QML_ROOT_DIR_URL) + "/card.qml"));
}

void QMLCard::closeEvent(QCloseEvent* event)
{
    deleteLater();
    messageQueue.pop();
    if (!messageQueue.empty()) {
        QQmlEngine* engine = nullptr;
        int x = 0;
        int y = 0;
        if (MainWindow::getMainWindow()) {
            engine = MainWindow::getMainWindow()->m_qmlEngine;
            auto pos = MainWindow::getMainWindow()->pos();
            x = pos.x() + MainWindow::getMainWindow()->width() - 10;
            y = pos.y() + 50;
        }
        auto tuple = messageQueue.front();
        currentCard = new QMLCard(std::get<0>(tuple), std::get<1>(tuple), engine);
        currentCard->show();
        currentCard->setPosition(x - currentCard->width(), y);
    } else {
        currentCard = nullptr;
    }
}

void QMLCard::closeCard()
{
    close();
}

void QMLCard::success(const QString& info)
{
    message(4, info);
}

void QMLCard::info(const QString& info)
{
    message(2, info);
}

void QMLCard::warning(const QString& warning)
{
    message(1, warning);
}

void QMLCard::error(const QString& error)
{
    message(0, error);
}

void QMLCard::message(int severity, const QString& message)
{
    if (currentCard == nullptr) {
        QQmlEngine* engine = nullptr;
        int x = 0;
        int y = 0;
        if (MainWindow::getMainWindow()) {
            engine = MainWindow::getMainWindow()->m_qmlEngine;
            auto pos = MainWindow::getMainWindow()->pos();
            x = pos.x() + MainWindow::getMainWindow()->width() - 10;
            y = pos.y() + 50;
        }
        currentCard = new QMLCard(severity, message, engine);
        currentCard->show();
        currentCard->setPosition(x - currentCard->width(), y);
    }
    messageQueue.emplace(severity, message);
    core::log(static_cast<core::LogLevel>((severity - 3) * -1), message.toStdString().c_str());
}

void QMLCard::resolvePromiseToCard(const model::DispatchQueue::ResultPipes& pipe, const std::string& successMessage, bool errorAsInfo, bool logSuccess, bool logError)
{
    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    if (logSuccess || logError) {
        QGuiApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
    }
    if (logSuccess) {
        pipe.success.observe_on(uiWorker).subscribe([successMessage](const std::string& m) {
            QGuiApplication::restoreOverrideCursor();
            if (!successMessage.empty()) {
                success(QString::fromStdString(successMessage));
            } else if (!m.empty()) {
                success(QString::fromStdString(m));
            }
        });
        pipe.error.observe_on(uiWorker).subscribe([](const std::string& e) {
            QGuiApplication::restoreOverrideCursor();
        });
    }
    if (logError) {
        pipe.error.observe_on(uiWorker).subscribe([errorAsInfo](const std::string& e) {
            QGuiApplication::restoreOverrideCursor();
            if (e.empty()) {
                error("Operation failed");
            } else {
                if (errorAsInfo) {
                    info(QString::fromStdString(e));
                } else {
                    error(QString::fromStdString(e));
                }
            }
        });
        pipe.success.observe_on(uiWorker).subscribe([](const std::string& e) {
            QGuiApplication::restoreOverrideCursor();
        });
    }
}
