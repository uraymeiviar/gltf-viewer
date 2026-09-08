// Copyright © 2020 UX3D GmbH. All rights reserved.

#include "gltf/Gltf.h"
#include "gltf/StateHistory.h"
#include "testHelpers/TestFileSystem.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
constexpr auto TestNamespace = "[tools.gltf.StateHistory]";

SCENARIO("StateHistory state machine checks", TestNamespace)
{
    GIVEN("history with 3 states")
    {
        Gltf gltf;
        StateHistory history(gltf);
        history.saveState();
        history.saveState();
        history.saveState();

        THEN("redo cannot be called")
        {
            CHECK_FALSE(history.canRedo());
        }
        THEN("undo can be called 2 times")
        {
            CHECK(history.canUndo());
            history.undo();
            CHECK(history.canUndo());
            history.undo();
            CHECK_FALSE(history.canUndo());

            AND_THEN("after 2 undos, redo can be called 2 times")
            {
                CHECK(history.canRedo());
                history.redo();
                CHECK(history.canRedo());
                history.redo();
                CHECK_FALSE(history.canRedo());
            }

            WHEN("another state is saved after undo, redo can not be called")
            {
                history.saveState();
                CHECK_FALSE(history.canRedo());
            }
        }
    }
}
SCENARIO("StateHistory value update checks", TestNamespace)
{
    Gltf gltf;
    StateHistory history(gltf);

    GIVEN("gltf with one node")
    {
        REQUIRE(gltf.create<Node>() != nullptr);
        auto& nodes = gltf.getNodes();
        REQUIRE(nodes.size() > 0);
        uint32_t id = nodes.front().m_ID;

        history.saveState();

        WHEN("node name is changed")
        {
            nodes.front().setName("Peter");
            REQUIRE(nodes.front().getName() == "Peter");

            history.saveState();

            AND_WHEN("undo is called")
            {
                history.undo();

                THEN("node name is back to nothing")
                {
                    CHECK(nodes.front().getName() == "");
                }
                AND_THEN("id is still the same")
                {
                    CHECK(nodes.front().m_ID == id);
                }
            }
        }
    }
    GIVEN("gltf with one named image")
    {
        Image* pImage = gltf.create<Image>();
        REQUIRE(pImage != nullptr);

        // URI required for undo to work properly
        core::string uri = UX3D_TEST_RESOURCES "/BoomBoxWithAxes_baseColor.png";
        pImage->setUri(uri);
        REQUIRE(pImage->getUri() == uri);
        REQUIRE(pImage->loadDataToMemory(filesystem::Dir(filesystem::TestFileSystem::instance())).derived == gltf::GltfResult::Success);

        core::string name = "I am the test name";
        pImage->setName(name);
        history.saveState();

        WHEN("state is saved")
        {
            history.saveState();

            AND_WHEN("undo is called")
            {
                history.undo();

                THEN("there is still one image with the correct name")
                {
                    REQUIRE(gltf.getImages().size() == 1);
                    CHECK(gltf.getImages().front().getName() == name);
                }
            }
        }
    }
}
}
