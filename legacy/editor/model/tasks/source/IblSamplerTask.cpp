// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#include "model/tasks/IblSamplerTask.h"

#include "filesystem/Dir.h"
#include "filesystem/File.h"
#include "image/Image.h"
#include "imageformats/imageformats.h"
#include "math/Math.h"

using namespace ux3d;
using namespace ux3d::model;
using namespace ux3d::model::tasks;

IblSamplerTask::IblSamplerTask(const filesystem::Path& _pathToHdr, const filesystem::Path& _pathToEnvironments)
    : Task(nextId())
    , pathToHdr(_pathToHdr)
{
    future = std::async(std::launch::async, [=]() -> filesystem::Path {
        // sample
        core::string environmentMapName = filesystem::File(_pathToHdr).getStem();
        filesystem::Path outputPath = _pathToEnvironments / environmentMapName;

        // ensure the path exists
        filesystem::Dir(outputPath).create();

        // store the .hdr file with the environment map (allows us to resample if ktx2 changes)
        filesystem::File inputHdrFile = _pathToHdr;
        core::Vector<uint8_t> hdrFileData;
        inputHdrFile.load(hdrFileData);

        // check if image is valid hdr file
        image::Image outImage;
        if (core::Failed(imageformats::decodeImage(hdrFileData, outImage))) {
            // It is enough to set this task to done and simply return an empty path.
            // When all env maps are queried we simply collect all files in the respective folder.
            // Therefore not adding it to the folder here is enough to avoid the import.
            setProgress(1.0f);
            setDone();
            core::logInfo("Invalid HDR file. Aborting import.");
            return filesystem::Path();
        }

        filesystem::File outputHdrFile = outputPath / inputHdrFile.getName();
        outputHdrFile.save(hdrFileData);

        setProgress(1.0f);
        setDone();
        core::logInfo("HDR Import: environment map successfully added to Gestaltor.");
        return outputPath;
    });
}

TaskId IblSamplerTask::nextId()
{
    static TaskId currentId = InvalidTaskId;
    return ++currentId;
}

IblSamplerTaskFactory& IblSamplerTaskFactory::getInstance()
{
    static IblSamplerTaskFactory instance;
    return instance;
}
