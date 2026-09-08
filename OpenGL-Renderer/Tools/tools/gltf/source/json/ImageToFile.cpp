// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/ImageToFile.h"
#include "filesystem/File.h"
#include "gltf/Image.h"
#include "gltf/StorageTypeConverter.h"

ux3d::gltf::ImageToFile::ImageToFile(const Image& _image, const filesystem::Dir& _outputDirectory)
    : m_image(_image)
    , m_outputDirectory(_outputDirectory)
{
}

ux3d::gltf::ConversionResult ux3d::gltf::ImageToFile::write(const core::string& _fileName)
{
    if (!hasValidUri() && _fileName == "") {
        return ConversionError::FileSaveFailure;
    }

    ConversionResult result = StorageTypeConverter::encodeImage(m_image, m_imageData);
    if (core::Failed(result)) {
        return result;
    }

    result = writeDataToFile(_fileName);
    if (core::Failed(result)) {
        return result;
    }

    return ConversionResult::Success;
}

bool ux3d::gltf::ImageToFile::hasValidUri()
{
    return false == m_image.getUri().empty();
}

ux3d::gltf::ConversionResult ux3d::gltf::ImageToFile::writeDataToFile(const core::string& _fileName)
{
    filesystem::File file(m_outputDirectory.getFilesystem());

    if (_fileName.empty() == false) {
        file = m_outputDirectory / _fileName;
    } else if (m_image.getUri().empty() == false) {
        file = m_outputDirectory / m_image.getUri();
    } else {
        file = m_outputDirectory / ("image_" + std::to_string(m_image.m_ID));
    }
    const filesystem::Dir finalDirectory = file.getParent();
    if (core::Failed(finalDirectory.create())) {
        return ConversionError::FileSaveFailure;
    }

    return core::Succeeded(file.save(m_imageData)) ? ConversionResult::Success : ConversionError::FileSaveFailure;
}
