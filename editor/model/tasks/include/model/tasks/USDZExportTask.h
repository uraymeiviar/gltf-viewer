// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝, David Labode

#pragma once

#include "Process.h"
#include "Task.h"
#include "filesystem/Path.h"

namespace ux3d::model::tasks {
class USDZExportTask : public Task<void> {
public:
    USDZExportTask(Process* process, const filesystem::Path& inputGltfPath, const filesystem::Path& outputUsdzPath);

    ~USDZExportTask();

private:
    Process* m_process;
    static TaskId nextId();
};

class USDZExportTaskFactory : public TaskFactory<USDZExportTask> {
public:
    static USDZExportTaskFactory& getInstance();
};
}
