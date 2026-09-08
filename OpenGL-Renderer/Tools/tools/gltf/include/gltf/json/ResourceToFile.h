// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ConversionResult.h"
#include "core/String.h"
#include "core/Vector.h"
#include "filesystem/File.h"

namespace ux3d::gltf {
class Buffer;
class Image;
class AudioSource;
struct ExportOptions;

class ResourceToFile {
public:
    ResourceToFile(const filesystem::File& _outputFile, const ExportOptions& _exportOptions)
        : m_outputFile(_outputFile)
        , m_exportOptions(_exportOptions)
    {
    }

    ConversionResult write(const Buffer& _buffer);
    ConversionResult write(core::MemoryRange<const uint8_t> bufferData);
    ConversionResult write(const Image& _image);
    ConversionResult write(const AudioSource& audioSource);

private:
    ConversionResult writeDataToFile();

    filesystem::File m_outputFile;
    core::Vector<uint8_t> m_data;

    const ExportOptions& m_exportOptions;
};
}
