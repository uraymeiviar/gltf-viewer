#include "gltf/GltfParsing.h"
#include "core/Logger.h"
#include "filesystem/File.h"
#include "gltf/ChildCollector.h"
#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/GltfUtils.h"
#include "gltf/ImportOptions.h"
#include "gltf/ResourceCollector.h"
#include "gltf/json/GltfFromBinary.h"
#include "gltf/json/GltfFromJson.h"
#include "gltf/json/GltfToGlb.h"
#include "gltf/json/GltfToJson.h"

constexpr auto GLTF_EXTENSION = ".gltf";
constexpr auto GLB_EXTENSION = ".glb";
constexpr auto GESTALT_EXTENSION = ".gestalt";

using namespace ux3d;
using namespace ux3d::gltf;

GltfFromFileResult GltfParsing::fromFile(
    Gltf& _outGltf,
    const filesystem::File& _file,
    const ImportOptions& _importOptions,
    bool _isGizmo)
{
    const core::string gltfExtension(GLTF_EXTENSION);
    const core::string glbExtension(GLB_EXTENSION);
    const core::string gestaltExtension(GESTALT_EXTENSION);

    if (_isGizmo) {
        core::logInfo("Loading '%s'", _file.toString().c_str());
    }

    GltfFromFileResult result;

    if (core::endsWith(core::toLower(_file.toString()), gltfExtension)) {
        result = fromGltfFile(_outGltf, _file, _importOptions);
    } else if (core::endsWith(core::toLower(_file.toString()), glbExtension)) {
        result = fromGlbFile(_outGltf, _file, _importOptions);
    } else if (core::endsWith(core::toLower(_file.toString()), gestaltExtension)) {
        result = fromGlbFile(_outGltf, _file, _importOptions);
    } else {
        result = GltfFromFileError::UnsupportedFileFormat;
    }

    if (result == GltfFromFileResult::Success) {
        if (_isGizmo) {
            core::debugLog(core::LogLevel::Info, "Successfully loaded '%s' %s", _file.toString().c_str(), "to content");
        }
    } else {
        core::logError("Could not load '%s'", _file.toString().c_str());
    }
    if (GltfUtils::deinterleaveAllAccessors(_outGltf) != GltfResult::Success) {
        return GltfFromFileError::InvalidGltf;
    }
    return result;
}

GltfToFileResult GltfParsing::toFile(
    const Gltf& _gltf,
    const filesystem::File& _file,
    ExportOptions _exportOptions)
{
    GltfToFileResult result;

    if (_exportOptions.exportCandidates.empty() == false) {
        ChildCollector childCollector(_exportOptions.exportCandidates);
        childCollector.collect(_exportOptions.exportCandidates);
    }

    switch (_exportOptions.fileContainer) {
    case FileContainer::Gltf:
        result = toGltfFile(_gltf, _file, _exportOptions);
        break;
    case FileContainer::Glb:
        result = toGlbFile(_gltf, _file, _exportOptions);
        break;
    default:
        result = GltfToFileError::UnsupportedFileFormat;
    }

    if (!_exportOptions.writeToMemory && _exportOptions.printLogOutput) {
        if (result == GltfToFileResult::Success) {
            core::logInfo("GltfParsing: Successfully saved '%s'", _file.toString().c_str());
        } else if (result != GltfToFileError::ResourceFileAlreadyExists) {
            core::logError("GltfParsing: Could not save '%s'", _file.toString().c_str());
        }
    }

    return result;
}

GltfFromFileResult GltfParsing::fromGltfFile(
    Gltf& _outGltf,
    const filesystem::File& _file,
    const ImportOptions& _importOptions)
{
    core::string gltfString;
    if (core::Failed(_file.load(gltfString))) {
        return GltfFromFileError::ReadFileFailure;
    }

    return fromString(_outGltf, _file.getParent(), gltfString, _importOptions);
}

GltfFromFileResult GltfParsing::fromGlbFile(
    Gltf& _outGltf,
    const filesystem::File& _file,
    const ImportOptions& _importOptions)
{
    core::HeapSlice<uint8_t> gltfBinary;
    if (core::Failed(_file.load(gltfBinary))) {
        return GltfFromFileError::ReadFileFailure;
    }

    return fromBinary(_outGltf, gltfBinary, _importOptions);
}

GltfToFileResult GltfParsing::toGltfFile(
    const Gltf& _gltf,
    const filesystem::File& _file,
    const ExportOptions& _exportOptions)
{
    ResourceCollector resourceCollector;

    GltfToJson converter(_gltf, resourceCollector, _exportOptions);
    ConversionResult result = converter.convert();

    if (result == ConversionError::AllocationFailure) {
        return GltfToFileError::AllocationFailure;
    }

    nlohmann::json jsonRoot = converter.getJson();
    if (jsonRoot.is_null() || jsonRoot.is_discarded()) {
        return GltfToFileError::JsonFailure;
    }

    GltfBinaryResolver binaryResolver(resourceCollector, _file, _exportOptions);
    ConversionResult resourceResult = binaryResolver.writeBinaryData(jsonRoot, _gltf);

    if (resourceResult == ConversionError::ResourceFileAlreadyExists) {
        // In this case, we do not want to write the glTF file
        return GltfToFileError::ResourceFileAlreadyExists;
    }

    if (core::Failed(resourceResult)) {
        core::logError("GltfParsing: Could not save binary data");
    }

    core::string gltfString;
    const int dumpMinified = _exportOptions.saveMinified ? -1 : 1;
    gltfString = jsonRoot.dump(dumpMinified, '\t', false, nlohmann::json::error_handler_t::ignore);

    if (core::Failed(_file.save(gltfString))) {
        core::logError("GltfParsing: Could not save text file");
        return GltfToFileError::SaveFileFailure;
    }

    return GltfToFileResult::Success;
}

GltfToFileResult GltfParsing::toGlbFile(
    const Gltf& _gltf,
    const filesystem::File& _file,
    const ExportOptions& _exportOptions)
{

    GltfToGlb converter(_gltf, _exportOptions);

    core::Vector<uint8_t> glbBuffer;

    ConversionResult result = converter.convert(glbBuffer);

    if (result == ConversionError::AllocationFailure) {
        return GltfToFileError::AllocationFailure;
    }

    if (core::Failed(_file.save(glbBuffer))) {
        return GltfToFileError::SaveFileFailure;
    }

    return GltfToFileResult::Success;
}

GltfFromFileResult GltfParsing::fromString(
    Gltf& _outGltf,
    const filesystem::Dir& _gltfDir,
    const core::string& _gltfString,
    const ImportOptions& _importOptions)
{
    GltfFromJson converter(_outGltf);
    ConversionResult result = converter.convert(_gltfString, _importOptions);

    if (result == ConversionError::AllocationFailure) {
        return GltfFromFileError::AllocationFailure;
    }

    if (result == ConversionError::InvalidJson) {
        return GltfFromFileError::InvalidJson;
    }

    if (result == ConversionError::LoadDataToMemoryFailure || result == ConversionError::FileNotFound || result == ConversionError::InvalidFilePath) {
        return GltfFromFileError::ReadFileFailure;
    }

    if (result != ConversionError::SuccessWithErrors && result != ConversionResult::Success) // remaining errors
    {
        return GltfFromFileError::InvalidGltf;
    }

    gltf::GltfResult dataResult = _outGltf.loadBinaryDataToMemory(_gltfDir, _importOptions.imageDir, false, _importOptions.loadImages);

    if (!dataResult) {
        return GltfFromFileError::ReadFileFailure;
    }

    return GltfFromFileResult::Success;
}

GltfFromFileResult GltfParsing::fromBinary(
    Gltf& _outGltf,
    const core::HeapSlice<uint8_t>& _data,
    const ImportOptions& _importOptions)
{
    GltfFromBinary converter(_outGltf, _importOptions);

    ConversionResult result = converter.convert(_data.getMemoryRange());
    if (result == ConversionError::AllocationFailure) {
        return GltfFromFileError::AllocationFailure;
    }

    if (result != ConversionError::SuccessWithErrors && result != ConversionResult::Success) // remaining errors
    {
        return GltfFromFileError::InvalidGltf; // todo: more specific error reporting
    }

    return GltfFromFileResult::Success;
}
