// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/BufferToFile.h"
#include "filesystem/File.h"
#include "gltf/Buffer.h"

ux3d::gltf::ConversionResult ux3d::gltf::BufferToFile::write()
{
    if (false == hasValidUri() && m_outputFileName.empty()) {
        core::logWarning("Cannot write buffer without uri");
        return ConversionError::FileSaveFailure;
    }

    if (ConversionResult result = fillData();
        core::Failed(result)) {
        return result;
    }

    if (ConversionResult result = writeDataToFile();
        core::Failed(result)) {
        return result;
    }

    return ConversionResult::Success;
}

bool ux3d::gltf::BufferToFile::hasValidUri()
{
    return false == m_buffer.getUri().empty();
}

ux3d::gltf::ConversionResult ux3d::gltf::BufferToFile::fillData()
{
    core::MemoryRange<const uint8_t> bufferData;
    m_buffer.getData(bufferData);

    if (bufferData.empty()) {
        return ConversionError::EmptyBuffer;
    }

    return m_data.assign(bufferData) ? ConversionResult::Success : ConversionError::AllocationFailure;
}

ux3d::gltf::ConversionResult ux3d::gltf::BufferToFile::writeDataToFile()
{
    filesystem::File file(m_outputDirectory.getFilesystem());

    if (m_outputFileName.empty() == false) {
        file = m_outputDirectory / m_outputFileName;
    } else if (m_buffer.getUri().empty() == false) {
        file = m_outputDirectory / m_buffer.getUri();
    } else {
        file = m_outputDirectory / ("buffer_" + std::to_string(m_buffer.m_ID));
    }

    const filesystem::Dir finalDirectory = file.getParent();
    if (core::Failed(finalDirectory.create())) {
        return ConversionError::FileSaveFailure;
    }

    return core::Succeeded(file.save(m_data)) ? ConversionResult::Success : ConversionError::FileSaveFailure;
}
