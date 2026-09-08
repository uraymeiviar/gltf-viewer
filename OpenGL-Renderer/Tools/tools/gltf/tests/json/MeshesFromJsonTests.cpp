// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Gltf.h"
#include "gltf/json/MeshesFromJson.h"
#include "gltf/json/PointerLookup.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
void prepareMeshWithDracoTest(nlohmann::json& _outMesh, Gltf& _outGltf, PointerLookup& _pointerLookup);

TEST_CASE("MeshesFromJson: KHR_draco_mesh_compression", "[tools.gltf.MeshesFromJson]")
{
    Gltf gltf;
    PointerLookup pointerLookup;
    nlohmann::json jsonMesh;
    prepareMeshWithDracoTest(jsonMesh, gltf, pointerLookup);

    MeshesFromJson fromJson(gltf, pointerLookup);
    fromJson.convert(jsonMesh);

    REQUIRE(gltf.getMeshes().size() == 1);
    const Mesh& mesh = gltf.getMeshes().front();

    REQUIRE(mesh.getPrimitives().size() == 1);
    const Primitive& primitive = mesh.getPrimitives().front();

    const auto& draco = primitive.getDracoMeshCompression();
    REQUIRE(draco.has_value());

    CHECK(draco->getBufferView() == &gltf.getBufferViews().front());

    CHECK(draco->getAttributes().size() == 4);
    CHECK(draco->getAttributes().front().name == "NORMAL");
    CHECK(draco->getAttributes().front().id == 1);
    CHECK(draco->getAttributes().at(1).name == "POSITION");
    CHECK(draco->getAttributes().at(1).id == 3);
    CHECK(draco->getAttributes().at(2).name == "TANGENT");
    CHECK(draco->getAttributes().at(2).id == 2);
    CHECK(draco->getAttributes().at(3).name == "TEXCOORD_0");
    CHECK(draco->getAttributes().at(3).id == 0);
}

void prepareMeshWithDracoTest(nlohmann::json& _outMesh, Gltf& _outGltf, PointerLookup& _pointerLookup)
{
    // the mesh references a bufferView, 5 accessors and a material
    REQUIRE(_pointerLookup.add(_outGltf.create<BufferView>()));
    REQUIRE(_pointerLookup.add(_outGltf.create<Accessor>()));
    REQUIRE(_pointerLookup.add(_outGltf.create<Accessor>()));
    REQUIRE(_pointerLookup.add(_outGltf.create<Accessor>()));
    REQUIRE(_pointerLookup.add(_outGltf.create<Accessor>()));
    REQUIRE(_pointerLookup.add(_outGltf.create<Accessor>()));
    REQUIRE(_pointerLookup.add(_outGltf.create<Material>()));

    REQUIRE(_outGltf.getBufferViews().size() >= 1);
    REQUIRE(_outGltf.getAccessors().size() >= 5);
    REQUIRE(_outGltf.getMaterials().size() >= 1);

    // taken directly from sample model BoomBox/glTF-Draco
    _outMesh = {
        { "name", "BoomBox" },
        { "primitives",
            { { { "attributes",
                    { { "TEXCOORD_0", 1u },
                        { "NORMAL", 2u },
                        { "TANGENT", 3u },
                        { "POSITION", 4u } } },
                { "indices", 0u },
                { "material", 0u },
                { "extensions",
                    { { "KHR_draco_mesh_compression",
                        { { "bufferView", 0u },
                            { "attributes",
                                { { "TEXCOORD_0", 0u },
                                    { "NORMAL", 1u },
                                    { "TANGENT", 2u },
                                    { "POSITION", 3u } } } } } } } } } }
    };
}
}
