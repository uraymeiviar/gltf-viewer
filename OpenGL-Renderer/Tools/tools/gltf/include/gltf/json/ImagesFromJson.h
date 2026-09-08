#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class Image;
class Ktx2;

class ImagesFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonImage) override;
    ConversionResult resolve(const nlohmann::json& _jsonImage, Image& _image);

private:
    ConversionResult convertStorageFormat(const nlohmann::json& _editorExtension, Image& _image); //Gestaltor Extension
    ConversionResult convertBackupUri(const nlohmann::json& _editorExtension, Image& _image); //Gestaltor Extension
    ConversionResult convertImageDimensions(const nlohmann::json& _editorExtension, Image& _image); //Gestaltor Extension
    ConversionResult convertJPEGMetaData(const nlohmann::json& _editorExtension, Image& _image); //Gestaltor Extension
    ConversionResult convertKTXMetaData(const nlohmann::json& _editorExtension, Image& _image); // Gestaltor Extension
    ConversionResult convertMapId(const nlohmann::json& _editorExtension, Image& _image); //Gestaltor Extension
};
}
