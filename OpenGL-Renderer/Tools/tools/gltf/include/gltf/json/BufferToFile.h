// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ConversionResult.h"
#include "core/String.h"
#include "core/Vector.h"
#include "filesystem/Dir.h"

namespace ux3d::gltf {
class Buffer;

class BufferToFile {
public:
    explicit BufferToFile(const Buffer& _buffer, const filesystem::Dir& _outputDirectory, const core::string& _outputFileName = "")
        : m_buffer(_buffer)
        , m_outputDirectory(_outputDirectory)
        , m_outputFileName(_outputFileName)
    {
    }

    ConversionResult write();

private:
    bool hasValidUri();
    ConversionResult fillData();
    ConversionResult writeDataToFile();

    const Buffer& m_buffer;
    filesystem::Dir m_outputDirectory;
    core::string m_outputFileName;
    core::Vector<uint8_t> m_data;
};
}
