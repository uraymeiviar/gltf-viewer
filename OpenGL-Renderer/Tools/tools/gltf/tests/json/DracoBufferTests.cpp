// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "../TestAssets.h"
#include "core/Logger.h"
#include "gltf/Gltf.h"
#include "gltf/mesh/DracoMeshCompression.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
namespace {
    constexpr auto TestNamespace = "[tools.gltf.KHR_draco_mesh_compression]";

    const gltf::Gltf& resolveTestAsset(TestAsset& _asset) { return _asset.get(); }
    const gltf::Gltf& resolveRoundtripTestAsset(TestAsset& _asset) { return _asset.getRoundtrip(); }
}

SCENARIO("KHR_draco_mesh_compression", TestNamespace)
{
    const Gltf& gltf = resolveTestAsset(BoomBoxDraco);
    const Gltf& gltfRoundtrip = resolveRoundtripTestAsset(BoomBoxDraco);

    SECTION("JSON values of KHR_draco_mesh_compression")
    {

        const auto& meshes = gltfRoundtrip.getMeshes();
        REQUIRE_FALSE(meshes.empty());

        const Mesh& mesh = meshes.front();

        const core::LinkedList<Primitive>& primitives = mesh.getPrimitives();
        REQUIRE_FALSE(primitives.empty());

        const Primitive& primitive = primitives.front();
        const auto& dracoMeshCompression = primitive.getDracoMeshCompression();

        REQUIRE(dracoMeshCompression.has_value());

        const core::Vector<DracoMeshCompression::DracoAttribute>& attributes = dracoMeshCompression->getAttributes();

        REQUIRE(attributes.size() == 4);

        bool texcoordAttributeFound = false;
        bool normalAttributeFound = false;
        bool tangentAttributeFound = false;
        bool positionAttributeFound = false;

        for (const DracoMeshCompression::DracoAttribute& dracoAttribute : attributes) {
            if (dracoAttribute.name == "TEXCOORD_0" && dracoAttribute.id == 0) {
                texcoordAttributeFound = true;
            }
            if (dracoAttribute.name == "NORMAL" && dracoAttribute.id == 1) {
                normalAttributeFound = true;
            }
            if (dracoAttribute.name == "TANGENT" && dracoAttribute.id == 2) {
                tangentAttributeFound = true;
            }
            if (dracoAttribute.name == "POSITION" && dracoAttribute.id == 3) {
                positionAttributeFound = true;
            }
        }

        CHECK(texcoordAttributeFound);
        CHECK(normalAttributeFound);
        CHECK(tangentAttributeFound);
        CHECK(positionAttributeFound);
    }

    SECTION("Check draco compressed buffer")
    {
        const auto& buffers = gltf.getBuffers();
        const auto& buffersRoundtrip = gltfRoundtrip.getBuffers();

        REQUIRE(buffers.size() == buffersRoundtrip.size());
        CHECK(buffers.size() > 0);

        for (size_t i = 0; i < buffers.size(); i++) {

            core::MemoryRange<const uint8_t> mem;
            buffers.at(i).getData(mem);

            core::MemoryRange<const uint8_t> memRoundtrip;
            buffersRoundtrip.at(i).getData(memRoundtrip);

            size_t alignA = (4 - (mem.size() % 4)) % 4;
            size_t alignB = (4 - (memRoundtrip.size() % 4)) % 4;

            REQUIRE(mem.size() + alignA == memRoundtrip.size() + alignB);
            CHECK(mem.size() > 0);

            for (size_t j = 0; j < mem.size() - 4; j += mem.size() / 10) {
                CHECK(memRoundtrip[j] == mem[j]);
            }
        }
    }
}
}
