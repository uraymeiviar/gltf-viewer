// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"

namespace ux3d::gltf::tests {
class TestAsset {
public:
    explicit TestAsset(core::string _filename, FileContainer _fileContainer = FileContainer::Gltf);

    const gltf::Gltf& get();
    const gltf::Gltf& getRoundtrip();
    const gltf::Gltf& getTransFormatRoundtrip();
    const gltf::Gltf& getMerged();

    void checkLoadedSavedFilesIdentical();
    void compareAssetToGltf(const Gltf& _gltf);

private:
    const core::string m_Filename;
    const FileContainer m_FileContainer = FileContainer::Gltf;

    bool m_LoadComplete;
    bool m_RoundtripComplete;
    bool m_TransFormatRoundtripComplete;
    bool m_MergedComplete;

    gltf::Gltf m_SimpleLoad;
    gltf::Gltf m_Roundtrip;
    gltf::Gltf m_TransFormatRoundtrip;
    gltf::Gltf m_Merged;

    void initSimple();
    void initRoundtrip();
    void initTransFormatRoundtrip();
    void initMerged();

    static void checkFilesIdentical(const core::string& _pathOriginal, const core::string& _pathCompare);
};

static TestAsset BoomBoxWithAxes("BoomBoxWithAxes.gltf");
static TestAsset AnimatedCube("AnimatedCube.gltf");
static TestAsset MorphCube("AnimatedMorphCube.gltf");
static TestAsset Monster("Monster.gltf");
static TestAsset Duck("Duck.gltf");
static TestAsset DuckEmbedded("Duck_Embedded.gltf");
static TestAsset DuckBinary("Duck.glb", FileContainer::Glb);
static TestAsset SparseAccessor("SimpleSparseAccessor.gltf");
static TestAsset BoomBoxDraco("BoomBoxDraco.gltf");
static TestAsset ExtSparse("sparse.gltf");
static TestAsset ExtSparseConfliciting("sparseConflicting.gltf");
static TestAsset ExtDraco("CesiumMilkTruck.gltf");
static TestAsset ExtMaterialVariants("AvocadoMaterialVariants.gltf");
}
