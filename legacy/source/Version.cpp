// Copyright © 2020 UX3D GmbH. All rights reserved.

#include "Version.h"
#include "Constants.h"
#include "widgets/MainWindow.h"
#include <QFile>

using namespace ux3d;
using namespace ux3d::editor;

Version::Version()
    : m_releaseVersion(constants::releaseVersion)
{
    QFile buildIdFile(UX3D_EDITOR_RESOURCES_DIR "/build_id");
    if (buildIdFile.open(QIODevice::ReadOnly)) {
        m_buildId = buildIdFile.readLine().trimmed().toStdString();
    }
}

Version& Version::get()
{
    static Version instance;
    return instance;
}

Version::operator std::string()
{
    std::string result = m_releaseVersion;
    return result;
}

bool Version::isDevelopmentVersion() const
{
    return true;
}
