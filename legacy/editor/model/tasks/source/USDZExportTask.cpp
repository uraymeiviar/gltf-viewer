// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝, David Labode

#include "model/tasks/USDZExportTask.h"
#include "core/Logger.h"
#include "core/Platform.h"
#include "filesystem/File.h"
#include "filesystem/FileSystem.h"

using namespace ux3d;
using namespace ux3d::model;
using namespace ux3d::model::tasks;

USDZExportTask::USDZExportTask(Process* process, const filesystem::Path& inputGltfPath, const filesystem::Path& outputUsdzPath)
    : Task(nextId())
    , m_process(process)
{
    future = std::async(std::launch::async, [=]() {
        setProgress(0.0f);

        // construct a temporary output dir so that the usdz export doesn't have sandbox issues
        filesystem::File tmpOutputFile = inputGltfPath.getFilesystem().getTempPath() / outputUsdzPath.getName();

#ifdef UX3D_IS_WINDOWS
        m_process->setProgram("usd_from_gltf.exe");
#elif UX3D_IS_LINUX
        m_process->setProgram("usd_from_gltf");
#elif UX3D_IS_APPLE
        m_process->setProgram("usd_from_gltf");
#else
#error
#endif
        m_process->addArgument(inputGltfPath.toString());
        m_process->addArgument(tmpOutputFile.toString());
        m_process->addArgument("--png_level");
        m_process->addArgument("1");
        m_process->addArgument("--image_limit_total");
        m_process->addArgument("0");
        m_process->run();

        // Read the usdz data from the temp location and store it to the desired output.
        // This ensures the sandbox configuration from Gestaltor can be used when writing the
        // output usdz
        core::Vector<uint8_t> usdzData;
        if (Failed(tmpOutputFile.load(usdzData))) {
            core::logError("USDZ Export: could not get results from export task");
            return;
        }
        tmpOutputFile.remove();

        filesystem::File outputFile = outputUsdzPath;
        if (Failed(outputFile.save(usdzData))) {
            core::logError("USDZ Export: could not write usdz file");
            return;
        }

        setProgress(1.0f);
        setDone();
    });
}

USDZExportTask::~USDZExportTask()
{
    delete m_process;
}

TaskId USDZExportTask::nextId()
{
    static TaskId currentId = InvalidTaskId;
    return ++currentId;
}

USDZExportTaskFactory& USDZExportTaskFactory::getInstance()
{
    static USDZExportTaskFactory instance;
    return instance;
}
