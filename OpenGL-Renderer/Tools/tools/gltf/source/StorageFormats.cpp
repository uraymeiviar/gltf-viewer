#include "gltf/StorageFormats.h"
#include "core/Logger.h"

using namespace ux3d;
using namespace ux3d::gltf;

core::string gltf::storageFormatToString(StorageFormat _format)
{
    switch (_format) {
    case StorageFormat::Undefined:
        return "undefined";
    case StorageFormat::NotStored:
        return "notStored";
    case StorageFormat::File:
        return "file";
    case StorageFormat::Embedded:
        return "embedded";
    case StorageFormat::BufferView:
        return "bufferView";
    default:
        break;
    }

    core::logError("No matching StorageFormat with value %d was found.", _format);
    return "";
}

StorageFormat gltf::storageFormatFromString(const core::string& _formatString)
{
    if (_formatString == "undefined") {
        return StorageFormat::Undefined;
    }
    if (_formatString == "notStored") {
        return StorageFormat::NotStored;
    }
    if (_formatString == "file") {
        return StorageFormat::File;
    }
    if (_formatString == "embedded") {
        return StorageFormat::Embedded;
    }
    if (_formatString == "bufferView") {
        return StorageFormat::BufferView;
    }

    core::logError("No matching StorageFormat was found for string '%s'", _formatString.c_str());
    return StorageFormat::Undefined;
}
