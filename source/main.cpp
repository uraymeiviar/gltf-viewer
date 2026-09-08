
#include "Constants.h"
#include "GestaltorApplication.h"
#include "Preferences.h"
#include "Version.h"
#include "helpers/QtFileSystem.h"
#include "model/ProjectManager.h"
#include "widgets/MainWindow.h"

#include "core/Logger.h"
#include "core/MemoryUtilities.h"
#include "core/Platform.h"
#include "core/String.h"
#include "licenses/gplv3.h"
#include "licenses/lgplv3.h"
#include "logging/ConsoleLog.h"
#include "logging/FileLogger.h"
#include "logging/VSConsoleLog.h"

#include "helpers/CommandLineParsing.h"
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFont>
#include <QFontDatabase>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QString>
#include <QSurfaceFormat>
#include <fstream>
#include <iostream>

#ifdef UX3D_IS_WINDOWS
#include <Windows.h>
#endif

using namespace ux3d;
using namespace ux3d::editor;

void initializeQApplication();
void prepareQApplication();
void writeQtLicenseFile();

void myMessageOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char* file = context.file ? context.file : "";
    const char* function = context.function ? context.function : "";
    switch (type) {
    case QtDebugMsg:
        core::debugLog(core::LogLevel::Info, "Qt Debug: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtInfoMsg:
        core::debugLog(core::LogLevel::Info, "Qt: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtWarningMsg:
        core::debugLog(core::LogLevel::Warning, "Qt: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtCriticalMsg:
        core::debugLog(core::LogLevel::Error, "Qt: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtFatalMsg:
        core::debugLog(core::LogLevel::Fatal, "Qt: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    }
}

int main(int argumentCount, char* arguments[])
{
    // heap debugging should only be enabled in debug configuration as it only gives meaningful reports with debug info
#ifndef NDEBUG
    core::initHeapDebugging();
    qInstallMessageHandler(myMessageOutput);
#endif

    // Allocate a console to this program and hide it. This will prevent consoles from spawning multiple times during runtime.
#ifdef UX3D_IS_WINDOWS
    AllocConsole();
    ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

    prepareQApplication();
    GestaltorApplication app(argumentCount, arguments);

    QQuickStyle::setStyle(QString("Style"));
    QQuickStyle::setFallbackStyle("Material");

    QFont font("Open Sans");
    font.setPixelSize(13);
    QApplication::setFont(font);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::GraphicsApi::OpenGL);

    const QString tempDataPath = QStandardPaths::standardLocations(QStandardPaths::TempLocation).first();
    const QDateTime dateTime = QDateTime::currentDateTime();
    core::string logFilename = constants::appName;
    logFilename += "_" + dateTime.toString("dd-MM-yyyy_hh-mm-ss").toStdString();
    logFilename += ".log";
    filesystem::File logFile = QtFileSystem::instance().toFile(tempDataPath.toStdString()) / logFilename;

    logging::ConsoleLog consoleLogger;
    logging::VSConsoleLog visualStudioLogger;
    logging::FileLogger fileLogger(logFile);
    core::addLoggerCallback(&fileLogger);
    if (Version::get().isDevelopmentVersion()) {
        core::addLoggerCallback(&consoleLogger);
        core::addLoggerCallback(&visualStudioLogger);
    }

    writeQtLicenseFile();

    int returnCode = 1;

    CommandLineParsing cmdParsing;
    cmdParsing.parse(app);

    initializeQApplication();

    MainWindow window(nullptr, logFile.toString());
    QObject::connect(&window, &MainWindow::exited, &app, &GestaltorApplication::quit, Qt::QueuedConnection);
    QObject::connect(&app, &GestaltorApplication::openFile, &window, &MainWindow::openFile);

    window.setWindowFlag(Qt::CustomizeWindowHint, true);
    window.setWindowFlag(Qt::WindowFullscreenButtonHint, false);

    window.show();

    returnCode = GestaltorApplication::exec();

    return returnCode;
}

void prepareQApplication()
{
    QApplication::setApplicationName(constants::appName); // you have to set the applicatiopn name before the appversion
    // cause otherwise the standard app location path used in license is wrong.
    // (version.get queries the license.)
    QApplication::setApplicationVersion(QString::fromStdString(Version::get()));

#ifdef UX3D_IS_LINUX
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#endif
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

void initializeQApplication()
{
    QFontDatabase::addApplicationFont(":/resources/fonts/OpenSans-Regular.ttf");
    QFontDatabase::addApplicationFont(":/resources/fonts/OpenSans-Bold.ttf");

    QPalette palette = QApplication::palette();
    palette.setColor(QPalette::Window, QColor(constants::defaultBackgroundColor));
    palette.setColor(QPalette::Base, QColor(constants::defaultBaseColor));
    palette.setColor(QPalette::Button, QColor(constants::defaultButtonColor));
    palette.setColor(QPalette::WindowText, QColor(constants::defaultTextColor));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(constants::defaultDisabledTextColor));
    palette.setColor(QPalette::Text, QColor(constants::defaultTextColor));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(constants::defaultDisabledTextColor));
    palette.setColor(QPalette::ButtonText, QColor(constants::defaultTextColor));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(constants::defaultDisabledTextColor));
    QApplication::setPalette(palette);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(0); // must be 0 for off-screen msaa resolve
    QSurfaceFormat::setDefaultFormat(format);
}

void writeQtLicenseFile()
{
    // QApplication is not initialized yet, so "Gestaltor" has to be appended to the applicationDataPath
    const QString applicationDataPath = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    if (QDir(QString(applicationDataPath + "/" + constants::appName)).exists() == false) {
        QDir().mkdir(QString(applicationDataPath + "/" + constants::appName));
    }
    if (QDir(QString(applicationDataPath + "/" + constants::appName + "/Licenses")).exists() == false) {
        QDir().mkdir(QString(applicationDataPath + "/" + constants::appName + "/Licenses"));
    }
    std::ofstream licenseFile;
    licenseFile.open(applicationDataPath.toStdString() + "/" + constants::appName + "/Licenses/QtLicenses.txt", std::ios::out);
    licenseFile << licenses::lgplv3_license;
    licenseFile << licenses::gplv3_license;
    licenseFile.close();
}
