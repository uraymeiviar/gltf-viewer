// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "TestAssets.h"
#include "catch2/catch.hpp"
#include "gltf/Gltf.h"
#include "gltf/GltfParsing.h"
#include "gltf/ImportOptions.h"
#include "testHelpers/TestFileSystem.h"

namespace ux3d::gltf::tests {
namespace {
    constexpr auto TestNamespace = "[tools.gltf.SelectiveExport]";
    const gltf::Gltf& resolveTestAsset(TestAsset& _Asset) { return _Asset.get(); }
    const auto& fs = filesystem::TestFileSystem::instance();
}

TEST_CASE("SelectiveExport: everything", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(Duck);
    const auto& nodes = gltf.getNodes();

    REQUIRE(nodes.size() == 3);

    const gltf::Node& node = nodes.front(); // all

    ExportOptions options { FileContainer::Gltf };
    options.exportCandidates.push_back(&node);
    GltfParsing::toFile(gltf, fs.toFile("Duck_Selecitve_Export_Complete.gltf"), options);

    gltf::Gltf gltfImport;
    ImportOptions importOptions;
    importOptions.conversionUsage = ConversionUsage::ValidGltf;
    GltfParsing::fromFile(gltfImport, fs.toFile("Duck_Selecitve_Export_Complete.gltf"), importOptions);

    const auto& nodeImport = gltfImport.getNodes();

    CHECK(nodeImport.size() == 3);
}

TEST_CASE("SelectiveExport: one node", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(Duck);
    const auto& nodes = gltf.getNodes();

    REQUIRE(nodes.size() == 3);

    const gltf::Node& node = nodes.at(2); // mesh

    ExportOptions options { FileContainer::Gltf };
    options.exportCandidates.push_back(&node);
    GltfParsing::toFile(gltf, fs.toFile("Duck_Selecitve_Export.gltf"), options);

    gltf::Gltf gltfImport;
    ImportOptions importOptions;
    importOptions.conversionUsage = ConversionUsage::ValidGltf;
    GltfParsing::fromFile(gltfImport, fs.toFile("Duck_Selecitve_Export.gltf"), importOptions);

    const auto& nodeImport = gltfImport.getNodes();

    CHECK(nodeImport.size() == 1);
    const gltf::Mesh* mesh = nodeImport.at(0).getMesh();
    REQUIRE(mesh != nullptr);

    CHECK(mesh->getName() == "LOD3spShape");

    const auto& primitiveList = mesh->getPrimitives();
    REQUIRE(primitiveList.size() == 1);
    CHECK(primitiveList.at(0).getMaterial()->getName() == "blinn3-fx");
}
}
