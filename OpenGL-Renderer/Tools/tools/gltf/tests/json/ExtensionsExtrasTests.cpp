// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#include "../TestAssets.h"
#include "catch2/catch.hpp"

namespace ux3d::gltf::tests {
constexpr auto TestNamespace = "[tools.gltf.ExtrasExtensions]";

TEST_CASE("Compare loaded and saved file: Sparse", TestNamespace)
{
    //ExtSparse.checkLoadedSavedFilesIdentical();
}
TEST_CASE("Compare loaded and saved file: Draco", TestNamespace)
{
    ExtDraco.checkLoadedSavedFilesIdentical();
}
TEST_CASE("Compare merged and saved file with original: Sparse", TestNamespace)
{
    //const Gltf& merged = ExtSparse.getMerged();
    //ExtSparse.compareAssetToGltf(merged);
}
TEST_CASE("Compare merged and saved file with original: Draco", TestNamespace)
{
    const Gltf& merged = ExtDraco.getMerged();
    ExtDraco.compareAssetToGltf(merged);
}
TEST_CASE("Test conflicting extensions")
{
    /*const Gltf& merged = ExtSparseConfliciting.getMerged();
		auto& material = merged.getMaterials().at(0);
		CHECK(material.getPbrSpecularGlossiness().has_value());
		CHECK(material.getUnlit().has_value());
		CHECK_FALSE(material.getClearcoat().has_value());
		ExtSparse.compareAssetToGltf(merged);*/
}
}
