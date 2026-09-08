// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#pragma once

#include "Task.h"
#include "filesystem/Path.h"

namespace ux3d::model::tasks {
class IblSamplerTask : public Task<filesystem::Path> {
public:
    // don't use this constructor, instead use the factory to construct tasks
    IblSamplerTask(const filesystem::Path& pathToHdr, const filesystem::Path& pathToEnvironments);

private:
    static TaskId nextId();

public:
    const filesystem::Path pathToHdr;
};

class IblSamplerTaskFactory : public TaskFactory<IblSamplerTask> {
public:
    static IblSamplerTaskFactory& getInstance();
};
}
