#pragma once

#include "math/VectorTypes.h"

#include <QApplication>
#include <QColor>
#include <QString>
#include <optional>

namespace ux3d::editor {

class CommandLineParsing {
public:
    struct CaptureConfiguration {
        QString inputPath;
        QString outputPath;
        QString iblPath;

        int32_t width = 0;
        int32_t height = 0;
        std::optional<uint32_t> toneMapping;
        int32_t framerate = 60; //[FPS]
        std::optional<int32_t> numberOfFrames;
        int32_t sceneIndex = -1;
        std::optional<uint32_t> cameraIndex;
        std::optional<QColor> clearColor;
        bool alphaChannel = false;
        std::list<uint32_t> animationClips = std::list<uint32_t>();
        bool animationEnabled = false;
        std::optional<float> vFoV;
        std::optional<float> radius;
        std::optional<float> theta;
        std::optional<float> phi;
        std::optional<math::vec3> target;
    };

    CommandLineParsing() {};
    void parse(QApplication& app);
    const CaptureConfiguration& getCaptureConfiguration()
    {
        return m_renderingConfiguration;
    };

private:
    CaptureConfiguration m_renderingConfiguration;
    math::vec3 parseVec3(QString string, bool& success);
    std::list<uint32_t> parseIntegerList(QString string, bool& success);
};
}
