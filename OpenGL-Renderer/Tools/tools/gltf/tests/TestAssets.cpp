#include "TestAssets.h"
#include "gltf/ExportOptions.h"
#include "gltf/GltfParsing.h"
#include "gltf/ImportOptions.h"
#include "testHelpers/TestFileSystem.h"
#include <catch2/catch.hpp>
#include <fstream>

using namespace ux3d;
using namespace ux3d::gltf;
using namespace ux3d::gltf::tests;

TestAsset::TestAsset(core::string _filename, FileContainer _fileContainer)
    : m_Filename(std::move(_filename))
    , m_FileContainer(_fileContainer)
    , m_LoadComplete(false)
    , m_RoundtripComplete(false)
    , m_TransFormatRoundtripComplete(false)
    , m_MergedComplete(false)
{
}

const gltf::Gltf& TestAsset::get()
{
    initSimple();
    return m_SimpleLoad;
}

const gltf::Gltf& TestAsset::getRoundtrip()
{
    initRoundtrip();
    return m_Roundtrip;
}

const gltf::Gltf& TestAsset::getTransFormatRoundtrip()
{
    initTransFormatRoundtrip();
    return m_TransFormatRoundtrip;
}

const gltf::Gltf& TestAsset::getMerged()
{
    initMerged();
    return m_Merged;
}

void TestAsset::checkLoadedSavedFilesIdentical()
{
    initRoundtrip();
    checkFilesIdentical(UX3D_TEST_RESOURCES "/" + m_Filename, m_Filename);
}

void TestAsset::compareAssetToGltf(const Gltf& _gltf)
{
    initSimple();

    ExportOptions options { FileContainer::Gltf };
    options.conversionUsage = ConversionUsage::ValidGltf;
    options.writeTimeStamp = false;
    filesystem::File file(filesystem::TestFileSystem::instance(), m_Filename);
    auto result = GltfParsing::toFile(_gltf, file, options);
    REQUIRE(result.derived == GltfToFileResult::Success);

    checkFilesIdentical(UX3D_TEST_RESOURCES "/" + m_Filename, m_Filename);
}

void TestAsset::initSimple()
{
    if (m_LoadComplete) {
        return;
    }

    m_LoadComplete = true;
    filesystem::File file(filesystem::TestFileSystem::instance(), UX3D_TEST_RESOURCES "/" + m_Filename);
    ImportOptions importOptions;
    importOptions.conversionUsage = ConversionUsage::ValidGltf;
    REQUIRE(GltfParsing::fromFile(m_SimpleLoad, file, importOptions).derived == GltfFromFileResult::Success);
}

void TestAsset::initRoundtrip()
{
    if (m_RoundtripComplete) {
        return;
    }

    initSimple();

    m_RoundtripComplete = true;

    ExportOptions options { m_FileContainer };
    options.conversionUsage = ConversionUsage::ValidGltf;
    options.writeTimeStamp = false;
    filesystem::File file(filesystem::TestFileSystem::instance(), m_Filename);
    REQUIRE(GltfParsing::toFile(m_SimpleLoad, file, options).derived == GltfToFileResult::Success);

    ImportOptions importOptions;
    importOptions.conversionUsage = ConversionUsage::ValidGltf;
    REQUIRE(GltfParsing::fromFile(m_Roundtrip, file, importOptions).derived == GltfFromFileResult::Success);
}

void TestAsset::initTransFormatRoundtrip()
{
    if (m_TransFormatRoundtripComplete) {
        return;
    }

    initSimple(); //->m_SimpleLoad

    m_TransFormatRoundtripComplete = true;

    const core::string gltfExtension(".gltf");
    const core::string glbExtension(".glb");

    core::string intermediateFilename = m_Filename;

    if (core::endsWith(core::toLower(m_Filename), gltfExtension)) {
        intermediateFilename += glbExtension;
    } else {
        intermediateFilename += gltfExtension;
    }

    FileContainer transType = m_FileContainer == FileContainer::Gltf ? FileContainer::Glb : FileContainer::Gltf;

    ExportOptions options { transType };
    options.conversionUsage = ConversionUsage::ValidGltf;
    options.writeTimeStamp = false;
    filesystem::File interMediateFile(filesystem::TestFileSystem::instance(), intermediateFilename);
    REQUIRE(GltfParsing::toFile(m_SimpleLoad, interMediateFile, options).derived == GltfToFileResult::Success);

    ImportOptions importOptions;
    importOptions.conversionUsage = ConversionUsage::ValidGltf;
    REQUIRE(GltfParsing::fromFile(m_TransFormatRoundtrip, interMediateFile, importOptions).derived == GltfFromFileResult::Success);
}

void TestAsset::initMerged()
{
    if (m_MergedComplete) {
        return;
    }

    initSimple();

    m_MergedComplete = true;

    REQUIRE(m_Merged.merge(m_SimpleLoad));
}

void TestAsset::checkFilesIdentical(const core::string& _pathOriginal, const core::string& _pathCompare)
{
    std::ifstream original(_pathOriginal);
    std::ifstream compare(_pathCompare);
    std::string lineOriginal;
    std::string lineCompare;

    uint32_t lineNumber = 0;
    while (std::getline(original, lineOriginal)) {
        bool compareFileTooShort = std::getline(compare, lineCompare).fail();
        REQUIRE_FALSE(compareFileTooShort);

        lineOriginal = "(line " + std::to_string(lineNumber) + ") " + lineOriginal;
        lineCompare = "(line " + std::to_string(lineNumber) + ") " + lineCompare;

        REQUIRE(lineOriginal == lineCompare);
        lineNumber++;
    }

    bool compareFileTooLong = std::getline(compare, lineCompare).fail() == false;
    REQUIRE_FALSE(compareFileTooLong);
}
