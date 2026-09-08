// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#pragma once

#include <string>

namespace ux3d::model::tasks {

class Process {
public:
    virtual ~Process() = default;

    virtual void setProgram(const std::string&) = 0;
    virtual void addArgument(const std::string&) = 0;

    virtual std::string applicationDirPath() = 0;

    virtual void run() = 0;
};

}
