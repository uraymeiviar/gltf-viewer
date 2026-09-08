// Copyright © 2022 UX3D GmbH. All rights reserved. 🏝

#include "gtest/gtest.h"

#include "Gltfs.h"
#include "reactor/Json.h"

#include <iostream>

using namespace ux3d;
using namespace ux3d::reactor;

TEST(Json, for_each_match)
{
    const auto boxGltf = nlohmann::json::parse(box);

    std::vector<std::string> names;
    json::for_each_match("/nodes/\\d+/name", boxGltf, [&](const nlohmann::json::json_pointer& pointer) {
        names.push_back(boxGltf.at(pointer));
    });

    EXPECT_EQ(names[0], "first");
}

TEST(Json, keys)
{
    auto boxGltf = nlohmann::json::parse(box);
    auto keys = json::keys(boxGltf);
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "accessors") != keys.end());
}

TEST(Json, values)
{
    auto boxGltf = nlohmann::json::parse(box);
    auto values = json::values(boxGltf.flatten());
    EXPECT_TRUE(std::find(values.begin(), values.end(), "COLLADA2GLTF") != values.end());
}

TEST(Json, unzip)
{
    auto jsonData = R"({
        "key": "value"
    })"_json;
    auto unzipped = json::unzip(jsonData);
    auto keys = unzipped["keys"];
    auto values = unzipped["values"];
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "key") != keys.end());
    EXPECT_TRUE(std::find(values.begin(), values.end(), "value") != values.end());
}

TEST(Json, minimal_diff_array)
{
    auto boxGltf = nlohmann::json::parse(box);
    auto previousGltf = boxGltf;

    boxGltf["nodes"].erase(0);

    auto diff = json::minimal_diff(previousGltf, boxGltf);

    // Should only result in a single diff operation
    EXPECT_EQ(diff.size(), 1);

    // with the op code "remove"
    EXPECT_EQ(diff[0]["op"], "remove");

    // acting on the first node
    EXPECT_EQ(diff[0]["path"], "/nodes/0");

    // The resulting patch should make the two JSONs identical
    previousGltf = previousGltf.patch(diff);
    EXPECT_EQ(boxGltf, previousGltf);

    // erase intermediate element of an array
    boxGltf["accessors"].erase(1);
    diff = json::minimal_diff(previousGltf, boxGltf);
    previousGltf = previousGltf.patch(diff);
    EXPECT_EQ(boxGltf, previousGltf);

    // erase last element of an array
    boxGltf["accessors"].erase(1);
    diff = json::minimal_diff(previousGltf, boxGltf);
    previousGltf = previousGltf.patch(diff);
    EXPECT_EQ(boxGltf, previousGltf);

    // add element to array
    boxGltf["nodes"].push_back({ { "name", "testNode" } });
    diff = json::minimal_diff(previousGltf, boxGltf);
    previousGltf = previousGltf.patch(diff);
    EXPECT_EQ(boxGltf, previousGltf);

    // reset box gltf
    boxGltf = nlohmann::json::parse(box);
    previousGltf = boxGltf;

    // remove two elements
    boxGltf["accessors"].erase(0);
    boxGltf["accessors"].erase(1);
    diff = json::minimal_diff(previousGltf, boxGltf);
    previousGltf = previousGltf.patch(diff);
    EXPECT_EQ(boxGltf, previousGltf);

    boxGltf["meshes"][0]["primitives"].erase(0);
    diff = json::minimal_diff(previousGltf, boxGltf);
    previousGltf = previousGltf.patch(diff);
    EXPECT_EQ(boxGltf, previousGltf);
}

TEST(Json, minimal_diff_object)
{
    auto boxGltf = nlohmann::json::parse(box);
    auto previousGltf = boxGltf;
    auto diff = nlohmann::json::array();

    // Should only result in a single diff operation
    boxGltf.erase("asset");
    diff = json::minimal_diff(boxGltf, previousGltf);
    previousGltf = previousGltf.patch(diff);
    EXPECT_EQ(diff.size(), 1);

    // Removing the child of a property should produce a valid patch
    boxGltf.at("/nodes/0/children"_json_pointer).erase(0);
    diff = json::minimal_diff(boxGltf, previousGltf);
    EXPECT_NO_THROW(previousGltf = previousGltf.patch(diff));
}

TEST(Json, filter)
{
    auto object = R"({
        "array": [],
        "object": {},
        "primitive": "value"
    })"_json;

    object = json::filter(std::move(object), [](const nlohmann::json& value) {
        return value.is_object();
    });
    EXPECT_EQ(object.size(), 1);
    EXPECT_TRUE(object.contains("object"));

    auto array = R"([
        2,
        3,
        4
    ])"_json;

    array = json::filter(std::move(array), [](const nlohmann::json& value) {
        return value.get<int>() % 2 == 0;
    });
    EXPECT_EQ(array.size(), 2);
    EXPECT_EQ(array, R"([2, 4])"_json);
}

TEST(Json, merge_patch)
{
    auto first = R"({
        "title": "Title",
        "name": "Name"
    })"_json;

    auto second = R"({
        "uuid": "8d44c7f1-ab7a-4899-a001-25146e105a70",
        "name": "New Name"
    })"_json;

    auto merged = first;

    merged.merge_patch(second);

    EXPECT_TRUE(merged.contains("title"));
    EXPECT_EQ(merged.at("title"), "Title");
    EXPECT_TRUE(merged.contains("uuid"));
    EXPECT_EQ(merged.at("uuid"), "8d44c7f1-ab7a-4899-a001-25146e105a70");
    EXPECT_TRUE(merged.contains("name"));
    EXPECT_EQ(merged.at("name"), "New Name");
}

TEST(Json, flatten_on)
{
    auto tree = R"({
        "children": [
            {
                "pointer": "/nodes/0",
                "children": [
                    {
                        "pointer": "/nodes/1"
                    }
                ]
            },
            {
                "pointer": "/nodes/2",
                "children": []
            },
            {
            }
        ]
    })"_json;

    auto expected = R"({
        "/children/0": {
            "pointer": "/nodes/0",
            "children": [
                {
                    "pointer": "/nodes/1"
                }
            ]
        },
        "/children/0/children/0": {
            "pointer": "/nodes/1"
        },
        "/children/1": {
            "pointer": "/nodes/2",
            "children": []
        },
        "/children/2": {}
    })"_json;

    auto flattenedTree = json::flatten_on(tree, "/children/\\d+");

    auto diff = nlohmann::json::diff(flattenedTree, expected);
    EXPECT_TRUE(diff.empty());
}

TEST(Json, mergeObj)
{
    auto jsonA = R"({
        "obj0":
            {
                "elm0": "abc",
                "elm1": [1,2,3] ,
                "common": {"elm0":0,"elm1":1}
            },
        "obj2":
            {
                "obj2elm0": "def",
                "obj2elm1": []
            }        
    })"_json;

    auto jsonB = R"({
        "obj0":
            {
                "elm2": "abc",
                "elm3": [1,2,3],
                "common": {"elm2":2,"elm3":3}
            },
        "obj1":
            {
                "obj1elm0": "def",
                "obj1elm1": []
            }        
    })"_json;

    auto merged = json::merge(jsonA, jsonB);
    EXPECT_EQ(merged.size(), 3);
    EXPECT_TRUE(merged["obj0"].contains("elm0"));
    EXPECT_TRUE(merged["obj0"].contains("elm2"));
    EXPECT_TRUE(merged["obj0"].contains("common"));
    EXPECT_TRUE(merged["obj0"]["common"].size() == 4);
}

TEST(Json, mergeObjConflict)
{
    auto jsonA = R"({
        "obj0":
            {
                "elm0": "abc",
                "elm1": [1,2,3] ,
                "common": {"elm0":0,"elm1":1},
                "conflict": 1
            },
        "obj2":
            {
                "obj2elm0": "def",
                "obj2elm1": []
            }        
    })"_json;

    auto jsonB = R"({
        "obj0":
            {
                "elm2": "abc",
                "elm3": [1,2,3],
                "common": {"elm2":2,"elm3":3},
                "conflict": 2
            },
        "obj1":
            {
                "obj1elm0": "def",
                "obj1elm1": []
            }        
    })"_json;

    EXPECT_ANY_THROW(auto merged = json::merge(jsonA, jsonB, false));
    EXPECT_NO_THROW(auto merged = json::merge(jsonA, jsonB, true));
}

TEST(Json, resolveAllOf)
{
    auto json = R"(
    {
    "items": {
      "$schema": "http://json-schema.org/draft-07/schema",
      "allOf": [
        {
          "$schema": "http://json-schema.org/draft-07/schema",
          "allOf": [
            {
              "$schema": "http://json-schema.org/draft-07/schema",
              "properties": {
                "extensions": {
                  "$schema": "http://json-schema.org/draft-07/schema",
                  "additionalProperties": {
                    "type": "object"
                  },
                  "description": "Dictionary object with extension-specific objects.",
                  "properties": {},
                  "title": "Extension",
                  "type": "object"
                },
                "extras": {
                  "$schema": "http://json-schema.org/draft-07/schema",
                  "description": "Application-specific data.",
                  "gltf_sectionDescription": "**Implementation Note:** Although extras may have any type, it is common for applications to store and access custom data as key/value pairs. As best practice, extras should be an Object rather than a primitive value for best portability.",
                  "title": "Extras"
                }
              },
              "title": "glTF Property",
              "type": "object"
            }
          ],
          "properties": {
            "name": {
              "description": "The user-defined name of this object.",
              "gltf_detailedDescription": "The user-defined name of this object.  This is not necessarily unique, e.g., an accessor and a buffer could have the same name, or two accessors could even have the same name.",
              "type": "string"
            }
          },
          "title": "glTF Child of Root Property",
          "type": "object"
        }
      ],
      "description": "A buffer points to binary geometry, animation, or skins.",
      "properties": {
        "byteLength": {
          "description": "The length of the buffer in bytes.",
          "minimum": 1,
          "type": "integer"
        },
        "extensions": {},
        "extras": {},
        "name": {},
        "uri": {
          "description": "The uri of the buffer.",
          "format": "uriref",
          "gltf_detailedDescription": "The uri of the buffer.  Relative paths are relative to the .gltf file.  Instead of referencing an external file, the uri can also be a data-uri.",
          "gltf_uriType": "application",
          "type": "string"
        }
      },
      "required": [
        "byteLength"
      ],
      "title": "Buffer",
      "type": "object"
    }
    })"_json;

    auto jsonResolved = reactor::json::resolveAllOf(json);
    EXPECT_TRUE(jsonResolved.at(nlohmann::json::json_pointer("/items/properties/name")).contains("type"));
    EXPECT_TRUE(jsonResolved.at(nlohmann::json::json_pointer("/items/properties/extensions/additionalProperties")).contains("type"));
    EXPECT_TRUE(jsonResolved.at(nlohmann::json::json_pointer("/items/properties")).contains("extras"));
}

TEST(Json, findValue)
{
    auto jsonA = R"({
        "embedded": {
            "uuid": "23418f5d-fafa-4566-84f2-d13c6549ac2a"
        },
        "files": {
            "uuid": "f081575c-1727-4307-80e4-bcfade645810"
        },
        "glb": {
            "uuid": "ab01f6c2-7313-4d1f-95f7-1b4e37d66ace"
        },
        "gltf": {
            "asset": {
                "uuid": "6e67e106-db31-4d2d-9da6-6f92d7c98a2a"
            },
            "nodes": [
                {
                    "uuid": "756e5edf-3a4d-489c-8286-777f1b3fb226"
                },
                {
                    "uuid": "d04add2d-4b9d-4194-8f2f-d6d018d61283"
                }
            ],
            "scenes": [
                {
                    "uuid": "a7a817db-764e-45fc-b6d2-c1cf7a919971"
                }
            ],
            "uuid": "de4cd924-7d5f-4d48-bb29-6a686583f802"
        }
    })"_json;

    std::vector<std::string> result = json::findValue(jsonA, "a7a817db-764e-45fc-b6d2-c1cf7a919971");

    EXPECT_TRUE(result.size() == 1);
    EXPECT_TRUE(result[0] == "/gltf/scenes/0/uuid");
}
TEST(Json, tree_diff_add)
{
    auto source = R"({ "children": [] })"_json;
    auto target = R"({
        "children": [
            {
                "pointer": "/nodes/0",
                "children": [
                    {
                        "pointer": "/nodes/1",
                        "children": []
                    }
                ]
            }
        ]
    })"_json;

    auto diff = json::tree_diff(source, target);

    // Expect that we go into children after we visited parents
    EXPECT_EQ(diff[0]["path"], "/children/0");
    EXPECT_EQ(diff[1]["path"], "/children/0/children/0");
    // only the two modifications should appear in the diff
    EXPECT_EQ(diff.size(), 2);
}

TEST(Json, tree_diff_add_to_existing)
{
    auto source = R"({
      "children": [
        {
            "children": [
                {
                    "id": "child_id"
                }
            ],
            "id": "some_id"
        }
      ]
    })"_json;
    auto target = R"({
      "children": [
        {
            "children": [
                {
                    "id": "added_child_id"
                }
            ],
            "id": "added_id"
        },
        {
            "children": [
                {
                    "id": "child_id"
                }
            ],
            "id": "some_id"
        }
      ]
    })"_json;

    auto diff = json::tree_diff(source, target);

    // Expect that we go into children after we visited parents
    EXPECT_EQ(diff[0]["path"], "/children/0");
    EXPECT_EQ(diff[1]["path"], "/children/0/children/0");
    // only the two modifications should appear in the diff
    EXPECT_EQ(diff.size(), 2);
}

TEST(Json, tree_diff_remove)
{
    auto source = R"({
        "children": [
            {
                "pointer": "/nodes/0",
                "children": [
                    {
                        "pointer": "/nodes/1"
                    }
                ]
            }
        ]
    })"_json;

    auto target = R"({ "children": [] })"_json;

    auto diff = json::tree_diff(source, target);

    // Expect that we go into children first (in case of a removal)
    EXPECT_EQ(diff[0]["path"], "/children/0/children/0");
    EXPECT_EQ(diff[1]["path"], "/children/0");
    // Only the two modifications should appear in the diff
    EXPECT_EQ(diff.size(), 2);
}

TEST(Json, tree_diff_insertion_order)
{
    auto source = R"({ "children": [] })"_json;
    auto target = R"({
        "children": [
            {
                "id": "0",
                "children": [
                    { "id": "00" }
                ]
            },
            {
                "id": "1",
                "children": [
                    { "id": "10" }
                ]
            },
            {
                "id": "2",
                "children": [
                    { "id": "20" }
                ]
            }
        ]
    })"_json;

    auto diff = json::tree_diff(source, target);

    EXPECT_EQ(diff[0]["path"], "/children/0");
    EXPECT_EQ(diff[0]["value"]["id"], "2");
    EXPECT_EQ(diff[1]["path"], "/children/0/children/0");
    EXPECT_EQ(diff[1]["value"]["id"], "20");

    EXPECT_EQ(diff[2]["path"], "/children/0");
    EXPECT_EQ(diff[2]["value"]["id"], "1");
    EXPECT_EQ(diff[3]["path"], "/children/0/children/0");
    EXPECT_EQ(diff[3]["value"]["id"], "10");

    EXPECT_EQ(diff[4]["path"], "/children/0");
    EXPECT_EQ(diff[4]["value"]["id"], "0");
    EXPECT_EQ(diff[5]["path"], "/children/0/children/0");
    EXPECT_EQ(diff[5]["value"]["id"], "00");
}

TEST(Json, tree_diff_deletion_order)
{
    auto source = R"({
        "children": [
            { "id": "0" },
            { "id": "1" }
        ]
    })"_json;
    auto target = R"({ "children": [] })"_json;

    auto diff = json::tree_diff(source, target);

    EXPECT_EQ(diff[0]["path"], "/children/1");
    EXPECT_EQ(diff[1]["path"], "/children/0");
}

TEST(Json, tree_diff_move)
{
    auto source = R"({
        "children": [
            { "id": "0", "children": [] },
            {
                "id": "1",
                "children": [ { "id": "2" } ]
            }
        ]
    })"_json;
    auto target = R"({
        "children": [
            {
                "id": "0",
                "children": [
                    {
                        "id": "1",
                        "children": [ { "id": "2" } ]
                    }
                ]
            }
        ]
    })"_json;

    auto diff = json::tree_diff(source, target);

    EXPECT_EQ(diff[0]["path"], "/children/1");
    EXPECT_EQ(diff[1]["path"], "/children/0");
}
