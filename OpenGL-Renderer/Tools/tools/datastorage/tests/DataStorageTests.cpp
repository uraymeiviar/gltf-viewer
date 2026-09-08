// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "datastorage/DataStorage.h"
#include "nlohmann/json.hpp"
#include <catch2/catch.hpp>

using namespace ux3d;
using namespace ux3d::datastorage;

TEST_CASE("DataStorage: basic insert, at, erase cycle ", "[tools.datastorage.DataStorage]")
{
    // fill dummy data
    std::vector<uint8_t> data;
    data.assign(5, 42);

    DataStorage storage;
    auto key = storage.insert(data);

    // check that the data at the key is equal to the inserted data
    auto& storageData = storage.at(key);
    CHECK(std::equal(data.begin(), data.end(), storageData.begin()));

    storage.erase(key);

    CHECK_THROWS(storage.at(key));
}

TEST_CASE("DataStorage: rvalue insert", "[tools.datastorage.DataStorage]")
{
    // fill dummy data
    std::vector<uint8_t> data;
    data.assign(5, 42);

    DataStorage storage;
    auto key = storage.insert(std::move(data));

    // check that the data at the key is equal to the inserted data
    auto& storageData = storage.at(key);
    CHECK(std::equal(data.begin(), data.end(), storageData.begin()));
}

TEST_CASE("DataStorage: pointer and size insert", "[tools.datastorage.DataStorage]")
{
    // fill dummy data
    std::vector<uint8_t> data;
    data.assign(5, 42);

    DataStorage storage;
    auto key = storage.insert(data.data(), data.size());

    // check that the data at the key is equal to the inserted data
    auto& storageData = storage.at(key);
    CHECK(std::equal(data.begin(), data.end(), storageData.begin()));
}

TEST_CASE("DataStorage: complete usage example", "[tools.datastorage.DataStorage]")
{
    // Example
    DataStorage storage;

    // let's say we have some URI pointing to a file
    std::string uri = "path/to/file";

    // and pretend that we have loaded the data at the uri to memory
    std::vector<uint8_t> data;
    data.assign(5, 42);

    // now store the data in the datastorage and keep the ID in some json
    nlohmann::json files;
    files[uri] = storage.insert(data);

    // now when we need to access the data again, we can do so by using the files json
    const std::vector<uint8_t>& dataFromStorage = storage.at(files[uri]);
    CHECK(std::equal(data.begin(), data.end(), dataFromStorage.begin()));
}
