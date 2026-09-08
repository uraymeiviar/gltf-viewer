// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "mementos/Mementos.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <catch2/catch.hpp>
#include <initializer_list>
#include <memory>
#include <vector>

using namespace ux3d;
using namespace ux3d::mementos;

// Record values the observable emits into an `std::vector`.
template <typename T>
std::shared_ptr<std::vector<T>> record(const rxcpp::observable<T>& observable)
{
    const auto vector = std::make_shared<std::vector<T>>();
    observable.subscribe([weak = std::weak_ptr(vector)](const T& value) {
        if (auto vector = weak.lock(); vector != nullptr) {
            vector->push_back(value);
        }
    });
    return vector;
}

TEST_CASE("Mementos: basic functionality", "[tools.mementos.Mementos]")
{
    Mementos<int> mementos;
    const auto& head = record(mementos.head);

    mementos.commit(1);

    CHECK_THAT(*head, Catch::Equals<int>({ 1 }));
}

TEST_CASE("Mementos: rvalue commit", "[tools.mementos.Mementos]")
{
    Mementos<int> mementos;
    const auto& head = record(mementos.head);

    mementos.commit(std::move(1));

    CHECK_THAT(*head, Catch::Equals<int>({ 1 }));
}

TEST_CASE("Mementos: no commit", "[tools.mementos.Mementos]")
{
    Mementos<int> mementos;
    const auto& head = record(mementos.head);
    CHECK(head->empty());
}

TEST_CASE("Mementos: move head", "[tools.mementos.Mementos]")
{
    Mementos<int> mementos;
    const auto& head = record(mementos.head);

    mementos.commit(1);
    mementos.commit(2);
    CHECK_THAT(*head, Catch::Equals<int>({ 1, 2 }));

    mementos.moveBack();
    CHECK_THAT(*head, Catch::Equals<int>({ 1, 2, 1 }));

    mementos.moveForward();
    CHECK_THAT(*head, Catch::Equals<int>({ 1, 2, 1, 2 }));

    // move forward out of bounds is idempotent
    mementos.moveForward();
    CHECK_THAT(*head, Catch::Equals<int>({ 1, 2, 1, 2 }));

    // move backward out of bounds is idempotent
    mementos.moveBack();
    mementos.moveBack();
    CHECK_THAT(*head, Catch::Equals<int>({ 1, 2, 1, 2, 1 }));

    mementos.commit(3);
    CHECK(head->back() == 3);
}

TEST_CASE("Mementos: Check size", "[tools.mementos.Mementos]")
{
    Mementos<int> mementos;
    const auto& size = record(mementos.size);

    // ensure size is zero initially
    CHECK_THAT(*size, Catch::Equals<size_t>({ 0 }));

    // initialize the first vaild state
    mementos.commit(1);

    CHECK_THAT(*size, Catch::Equals<size_t>({ 0, 1 }));
}

TEST_CASE("Mementos: Check futureless", "[tools.mementos.Mementos]")
{
    Mementos<int> mementos;
    const auto& futureless = record(mementos.futureless);

    CHECK(futureless->back());

    mementos.commit(1);
    CHECK(futureless->back());

    mementos.moveBack();
    CHECK_FALSE(futureless->back());

    mementos.moveForward();
    CHECK(futureless->back());
}

TEST_CASE("Mementos: Check pastless", "[tools.mementos.Mementos]")
{
    Mementos<int> mementos;
    const auto& pastless = record(mementos.pastless);

    CHECK(pastless->back());

    mementos.commit(1);
    CHECK_FALSE(pastless->back());

    mementos.commit(1);
    mementos.moveBack();
    CHECK_FALSE(pastless->back());

    mementos.clear();
    CHECK(pastless->back());
}

TEST_CASE("Mementos: Usage example using JSON data", "[tools.mementos.Mementos]")
{
    Mementos<nlohmann::json> mementos;
    const auto& head = record(mementos.head);

    // let's pretend we loaded some json (e.g. glTF) from disk
    nlohmann::json json = R"(
    {
        "asset": {
            "version": "2.0"
        },
        "scene": 0,
        "scenes": [
            {
                "nodes": [
                    0
                ]
            }
        ],
        "nodes": [
            {
                "name": "old name"
            }
        ]
    }
    )"_json;

    // and add it as the first element to our history
    mementos.commit(json);

    // now we simulate what would happen if the user wanted to change a value in the json

    // first obtain the head as a copy
    nlohmann::json workingJson = head->back();

    // declare an operation on the json
    auto replaceOperation = [](nlohmann::json&& json, const std::string& jsonPath, const auto& newValue) -> nlohmann::json {
        // then modify the data somehow
        nlohmann::json patch = nlohmann::json::array({ { { "op", "replace" },
            { "path", jsonPath },
            { "value", newValue } } });
        return json.patch(patch);
    };
    // execute the operation to change the node's name
    workingJson = replaceOperation(std::move(workingJson), "/nodes/0/name", "new name");

    // finally commit to mementos
    mementos.commit(workingJson);

    CHECK(head->back()["nodes"][0]["name"] == "new name");
}
