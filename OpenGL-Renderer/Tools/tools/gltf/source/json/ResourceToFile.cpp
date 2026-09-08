// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/ResourceToFile.h"
#include "filesystem/Dir.h"
#include "filesystem/File.h"
#include "gltf/AudioSource.h"
#include "gltf/Buffer.h"
#include "gltf/ExportOptions.h"
#include "gltf/Image.h"
#include "gltf/StorageTypeConverter.h"

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult ResourceToFile::write(const Image& _image)
{
    ConversionResult result = StorageTypeConverter::encodeImage(_image, m_data);
    if (core::Failed(result)) {
        return result;
    }

    if (ConversionResult result = writeDataToFile();
        core::Failed(result)) {
        return result;
    }

    return ConversionResult::Success;
}

ConversionResult ResourceToFile::write(const AudioSource& audioSource)
{
    const core::Vector<uint8_t>* encodedData = audioSource.getEncodedBytes();
    if (encodedData != nullptr) {
        if (m_data.assign(*encodedData) == false) {
            return ConversionError::AllocationFailure;
        }
    }

    if (ConversionResult result = writeDataToFile();
        core::Failed(result)) {
        return result;
    }

    return ConversionResult::Success;
}

ConversionResult ResourceToFile::write(const Buffer& _buffer)
{
    core::MemoryRange<const uint8_t> bufferData;
    _buffer.getData(bufferData);

    return write(bufferData);
}

ConversionResult ResourceToFile::write(core::MemoryRange<const uint8_t> bufferData)
{
    if (bufferData.empty()) {
        return ConversionError::EmptyBuffer;
    }

    if (m_data.assign(bufferData) == false) {
        return ConversionError::AllocationFailure;
    }

    if (ConversionResult result = writeDataToFile();
        core::Failed(result)) {
        return result;
    }

    return ConversionResult::Success;
}

ConversionResult ResourceToFile::writeDataToFile()
{
    const filesystem::Dir finalDirectory = m_outputFile.getParent();
    if (core::Failed(finalDirectory.create())) {
        return ConversionError::FileSaveFailure;
    }

    if (m_outputFile.exists()) {
        core::debugLog(core::LogLevel::Warning, "File already exists: %s", m_outputFile.toString().c_str());

        if (m_exportOptions.overwriteResourceFiles) {
            //...however, we explicitely want to overwrite these resource files
            core::debugLog(core::LogLevel::Info, "File is overwritten.");
        } else {
            core::debugLog(core::LogLevel::Info, "Aborting operation. No resource files are overwritten.");
            return ConversionError::ResourceFileAlreadyExists;
        }
    }

    if (!m_exportOptions.writeToMemory) {
        core::logInfo("Writing to %s", m_outputFile.toString().c_str());
    }
    return core::Succeeded(m_outputFile.save(m_data)) ? ConversionResult::Success : ConversionError::FileSaveFailure;
}
