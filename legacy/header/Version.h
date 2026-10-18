// Copyright © 2020 UX3D GmbH. All rights reserved.

#pragma once

#include <string>

namespace ux3d::editor {
class Version {
public:
    static Version& get();
    operator std::string();

    Version(const Version&) = delete;

    const std::string& getReleaseVersion() const { return m_releaseVersion; }
    const std::string& getBuildId() const { return m_buildId; }

    bool isDevelopmentVersion() const;

private:
    std::string m_releaseVersion;
    std::string m_buildId;

    Version();
    ~Version() = default;
};
}
