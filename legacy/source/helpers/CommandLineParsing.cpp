#include "helpers/CommandLineParsing.h"
#include "core/Logger.h"
#include <QCommandLineOption>
#include <QCommandLineParser>

#include "Constants.h"
#include "Preferences.h"
#include "model/ProjectManager.h"

using namespace ux3d;
using namespace ux3d::editor;

void CommandLineParsing::parse(QApplication& app)
{
    QCommandLineParser parser;

    parser.setApplicationDescription(QString(constants::appName) + " - " + constants::appDescription + " - These command line options offer the possibility to automatically create renderings of glTF and glb files.");

    const QCommandLineOption inputFile({ "i", "input" }, "Path to the input .glb or .glTF file", "file");
    parser.addOption(inputFile);

    const QCommandLineOption renderOutputFile({ "o", "output" }, "Path to the rendered image optionally including the desired output format (.png or .jpeg)", "image");
    parser.addOption(renderOutputFile);

    const QCommandLineOption renderWidth({ "x", "width" }, "Width of the rendered image", "number");
    parser.addOption(renderWidth);

    const QCommandLineOption renderHeight({ "y", "height" }, "Height of the rendered image", "number");
    parser.addOption(renderHeight);

    const QCommandLineOption cameraIndex({ "c", "camera" }, "glTF camera index, if an aspect ratio is available for this camera, only x/width or y/height is required", "index");
    parser.addOption(cameraIndex);

    const QCommandLineOption sceneIndex({ "s", "scene" }, "glTF scene index", "index");
    parser.addOption(sceneIndex);

    const QCommandLineOption clearColor("clearcolor", "Color value in the format #RRGGBB (red, green blue) or #AARRGGBB (alpha, red, green, blue) used as background color (--clearcolor overwrites ibl background)", "color");
    parser.addOption(clearColor);

    const QCommandLineOption alphaChannel("alphaChannel", "If this flag is set, the alpha channel is included in the image file");
    parser.addOption(alphaChannel);

    const QCommandLineOption ibl("ibl", "Path to an .hdr file used for image-based lighting (ibl) and background (--clearcolor overwrites ibl background)", "image");
    parser.addOption(ibl);

    const QCommandLineOption toneMapping({ "t", "tonemapping" }, "Tonemapping that is applied to the rendered scene. None = 0, ACES Filmic (NARKOWICZ) = 1, ACES Filmic (HILL) = 2, ACES Filmic (HILL - Exposure Boost) = 3", "number", "None");
    parser.addOption(toneMapping);

    const QCommandLineOption animationClips({ "a", "animations" }, "Indices of animations that should be rendered, e.g. [1,3,4] or 2", "array of indices");
    parser.addOption(animationClips);

    const QCommandLineOption numberOfFrames({ "f", "frames" }, "Number of animation frames that should be rendered", "number");
    parser.addOption(numberOfFrames);

    const QCommandLineOption framerate({ "r", "framerate" }, "Framerate [frames per second] that is used when rendering multiple frames of an animation", "number", "60");
    parser.addOption(framerate);

    const QCommandLineOption radius("radius", "Distance from the camera to the target (used for orbit camera configuration)", "number");
    parser.addOption(radius);

    const QCommandLineOption target("target", "Target in world coordinates that the camera looks at in format [0.0,0.0,0.0] (used for orbit camera configuration)", "array of numbers");
    parser.addOption(target);

    const QCommandLineOption theta("theta", "Horizontal camera angle (90 degrees looks along the -X axis) (used for orbit camera configuration)", "angle");
    parser.addOption(theta);

    const QCommandLineOption phi("phi", "Vertical camera angle (90 degrees looks along the -Y axis) (used for orbit camera configuration)", "angle");
    parser.addOption(phi);

    const QCommandLineOption yFoV("yfov", "Vertical field of view [degrees] (forces perspective camera)", "angle");
    parser.addOption(yFoV);

    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    if (parser.positionalArguments().count() > 0) {
        QString filename = parser.positionalArguments().front();
        Preferences::getInstance()->setInitialFilename(filename);
    }

    if (parser.value(inputFile) != "") {
        Preferences::getInstance()->setInitialFilename(parser.value(inputFile));
        m_renderingConfiguration.inputPath = parser.value(inputFile);
    }

    if (parser.value(renderOutputFile) != "") {
        model::ProjectManager::getInstance()->setCMDMode(true);
        m_renderingConfiguration.outputPath = parser.value(renderOutputFile);
    }

    if (parser.isSet(renderWidth)) {
        bool success = false;
        m_renderingConfiguration.width = parser.value(renderWidth).toInt(&success);
        if (!success) {
            core::logWarning("Invalid argument: %s", renderWidth.names().at(0).toStdString().c_str());
        }
    }

    if (parser.isSet(renderHeight)) {
        bool success = false;
        m_renderingConfiguration.height = parser.value(renderHeight).toInt(&success);
        if (!success) {
            core::logWarning("Invalid argument: %s", renderHeight.names().at(0).toStdString().c_str());
        }
    }

    if (parser.isSet(cameraIndex)) {
        bool success = false;
        m_renderingConfiguration.cameraIndex = parser.value(cameraIndex).toInt(&success);
        if (!success) {
            core::logWarning("Invalid argument: %s", cameraIndex.names().at(0).toStdString().c_str());
        }
    }
    if (parser.isSet(sceneIndex)) {
        bool success = false;
        m_renderingConfiguration.sceneIndex = parser.value(sceneIndex).toInt(&success);
        if (!success) {
            core::logWarning("Invalid argument: %s", sceneIndex.names().at(0).toStdString().c_str());
        }
    }
    if (parser.isSet(clearColor)) {
        QColor parsedColor = QColor(parser.value(clearColor));
        if (parsedColor.isValid()) {
            m_renderingConfiguration.clearColor = parsedColor;
        } else {
            core::logWarning("Invalid argument: %s", clearColor.names().at(0).toStdString().c_str());
        }
    }
    if (parser.isSet(alphaChannel)) {
        m_renderingConfiguration.alphaChannel = true;
    }
    if (parser.isSet(ibl)) {
        if (false == QFile::exists(parser.value(ibl))) {
            core::logError("Invalid argument \"%s\": file doesn't exist.", parser.value(ibl).toStdString().c_str());
            app.exit();
        }
        m_renderingConfiguration.iblPath = parser.value(ibl);
    }
    if (parser.isSet(toneMapping)) {
        bool success = false;
        const uint32_t value = parser.value(toneMapping).toInt(&success);
        if (value < 4) {
            m_renderingConfiguration.toneMapping = value;
        } else {
            success = false;
        }
        if (!success) {
            core::logWarning("Invalid argument: %s", toneMapping.names().at(0).toStdString().c_str());
        }
    }
    if (parser.isSet(animationClips)) {
        bool success = false;
        m_renderingConfiguration.animationClips = parseIntegerList(parser.value(animationClips), success);
        if (!success) {
            core::logWarning("Invalid argument: %s", animationClips.names().at(0).toStdString().c_str());
        }
        m_renderingConfiguration.animationEnabled = true;
    }
    if (parser.isSet(numberOfFrames)) {
        bool success = false;
        m_renderingConfiguration.numberOfFrames = parser.value(numberOfFrames).toInt(&success);
        if (!success) {
            core::logWarning("Invalid argument: %s", numberOfFrames.names().at(0).toStdString().c_str());
        }
    }
    if (parser.isSet(framerate)) {
        bool success = false;
        m_renderingConfiguration.framerate = parser.value(framerate).toInt(&success);
        if (!success) {
            core::logWarning("Invalid argument: %s", framerate.names().at(0).toStdString().c_str());
        }
    }
    if (parser.isSet(theta)) {
        bool success = false;
        m_renderingConfiguration.theta = parser.value(theta).toFloat(&success);
        if (!success) {
            core::logWarning("Invalid argument: %s", theta.names().at(0).toStdString().c_str());
        }
    }
    if (parser.isSet(phi)) {
        bool success = false;
        m_renderingConfiguration.phi = parser.value(phi).toFloat(&success);
        if (!success) {
            core::logWarning("Invalid argument: %s", phi.names().at(0).toStdString().c_str());
        }
    }
    if (parser.isSet(radius)) {
        bool success = false;
        m_renderingConfiguration.radius = parser.value(radius).toFloat(&success);
        if (!success) {
            core::logWarning("Invalid argument: %s", radius.names().at(0).toStdString().c_str());
        }
    }
    if (parser.isSet(target)) {
        bool success = false;
        m_renderingConfiguration.target = parseVec3(parser.value(target), success);
        if (!success) {
            core::logWarning("Invalid argument: %s", target.names().at(0).toStdString().c_str());
        }
    }
    if (parser.isSet(yFoV)) {
        bool success = false;
        m_renderingConfiguration.vFoV = parser.value(yFoV).toFloat(&success);
        if (!success) {
            core::logWarning("Invalid argument: %s", yFoV.names().at(0).toStdString().c_str());
        }
    }
}

math::vec3 CommandLineParsing::parseVec3(QString string, bool& success)
{
    if (string.length() >= 7) {
        if (string.front() == '[' && string.back() == ']') {
            string.remove(0, 1);
            string.remove(string.length() - 1, 1);
            auto list = string.split(',');
            math::vec3 result = math::vec3(0.0f);
            for (int i = 0; i < list.length() && i < 3; ++i) {
                result[i] = list[i].toFloat(&success);
                if (success == false) {
                    return result;
                }
            }
            success = true;
            return result;
        }
    }
    success = false;
    return math::vec3(0.0f);
}

std::list<uint32_t> CommandLineParsing::parseIntegerList(QString string, bool& success)
{
    if (string.front() == '[' && string.back() == ']') {
        // parse a string holding a list of values, e.g. "[42,1,23]" or "[5]"
        string.remove(0, 1);
        string.remove(string.length() - 1, 1);
        QStringList list = string.split(',');
        std::list<uint32_t> result = std::list<uint32_t>();
        for (QString entry : list) {
            result.push_back(entry.toUInt(&success));
            if (success == false) {
                return result;
            }
        }
        success = true;
        return result;
    } else {
        // parse a string holding a single value, e.g. "42"
        return { string.toUInt(&success) };
    }
}
