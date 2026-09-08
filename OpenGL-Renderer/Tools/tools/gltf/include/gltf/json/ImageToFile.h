// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <utility>

#include "ConversionResult.h"
#include "core/String.h"
#include "core/Vector.h"
#include "filesystem/Dir.h"
#include "imageformats/imageformats.h"

namespace ux3d::gltf {
class Image;

class ImageToFile {
public:
    explicit ImageToFile(const Image& _image, const filesystem::Dir& _outputDirectory);
    ConversionResult write(const core::string& _fileName = "");

private:
    bool hasValidUri();
    ConversionResult writeDataToFile(const core::string& _fileName = "");

    const Image& m_image;
    filesystem::Dir m_outputDirectory;
    core::Vector<uint8_t> m_imageData;
};
}
