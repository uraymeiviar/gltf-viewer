// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "model/GltfModel.h"
#include "core/Logger.h"
#include "model/ProjectManager.h"
#include "operations/Schema.h"
#include "reactor/Gltf.h"
#include "reactor/ImageOperations.h"
#include "reactor/Json.h"
#include "reactor/Reactor.h"
#include "reactor/Schema.h"
#include "reactor/Utils.h"
#include "reactor/Validation.h"

#include <gltf_schema/gltf_single_json.h>

#include <functional>
#include <iostream>

using namespace ux3d;
using namespace ux3d::model;

Gltf::Gltf(const std::function<rxcpp::observable<nlohmann::json>(const rxcpp::observable<nlohmann::json>&)>& observeOn)
{

    auto schemas = gltf_schema::merged();
    auto gltfSchema = reactor::schema::resolveRefs(gltf_schema::merged(), "glTF.schema.json");
    auto linearizedSchema = reactor::schema::linearize(gltfSchema);
    m_linearizedSchema = linearizedSchema;
    m_operationsLibrary.loadPlugin(operations::gltfOperations(gltfSchema, linearizedSchema));
    m_operationsLibrary.loadPlugin(operations::advancedGltfOperations(gltfSchema, linearizedSchema));

    this->operationsSchema = m_operationsLibrary.schema;

    // Generate a list of glTF properties which are all child-of-root-objects, such as `meshes`
    std::vector<nlohmann::json::json_pointer> childOfRootObjects;
    {
        std::function<void(const nlohmann::json& object, nlohmann::json::json_pointer&, bool)> gatherChildOfRootPaths;
        gatherChildOfRootPaths = [&](const nlohmann::json& object, nlohmann::json::json_pointer& pointer, bool pushToPointer) {
            for (const auto& [key, value] : object.items()) {
                if (key == "$ref") {
                    if (value == "glTFChildOfRootProperty.schema.json") {
                        childOfRootObjects.push_back(pointer);
                    } else {
                        gatherChildOfRootPaths(schemas[value.get<std::string>()], pointer, false);
                    }
                } else if (object.is_object() || object.is_array()) {
                    if (pushToPointer) {
                        pointer.push_back(key);
                    }
                    gatherChildOfRootPaths(value, pointer, key == "properties");
                    if (pushToPointer) {
                        pointer.pop_back();
                    }
                }
            }
        };
        nlohmann::json::json_pointer pointer;
        gatherChildOfRootPaths(schemas["glTF.schema.json"], pointer, false);
    }

    std::vector<nlohmann::json::json_pointer> resourcesCategories;
    resourcesCategories.resize(childOfRootObjects.size());
    int counter = 0;
    for (const auto& pointer : childOfRootObjects) {
        std::string strPointer = pointer.to_string();
        if (linearizedSchema.contains(strPointer) && linearizedSchema.at(strPointer).contains("ux3d_resources")) {
            resourcesCategories[linearizedSchema.at(strPointer).at("ux3d_resources").get<int>()] = pointer;
            ++counter;
        }
    }
    resourcesCategories.resize(counter);
    resourcesCategories.push_back("/extensions/KHR_xmp_json_ld/packets"_json_pointer);

    nlohmann::json filteredSchema;
    for (const auto& [key, value] : linearizedSchema.items()) {
        if (value.contains("ux3d_pathPrefix")) {
            if (value["ux3d_pathPrefix"] == "/scenes" || value["ux3d_pathPrefix"] == "/nodes"
                || value["ux3d_pathPrefix"] == "/materials" || value["ux3d_pathPrefix"] == "/meshes"
                || value["ux3d_pathPrefix"] == "/cameras" || value["ux3d_pathPrefix"] == "/extensions/KHR_lights_punctual/lights"
                || value["ux3d_pathPrefix"] == "/skins") {
                filteredSchema[key] = value;
            }
        }
    }

    canUndo = m_mementos.size.map([](size_t n) { return n > 1; });
    canRedo = m_mementos.canMoveForward;

    this->headRaw = m_mementos.head;

    // converts newGltf to an observable of integers, as this is the kind of observable required by window toggle
    this->newGltfIndex = m_mementos.instant
                             .filter([](const mementos::Instant<nlohmann::json>& instant) {
                                 return instant.future.empty() && instant.past.size() == 1;
                             })
                             .scan(0, [](int seed, const auto& v) {
                                 return seed + 1;
                             })
                             .publish(0)
                             .ref_count()
                             .filter([](int index) { return index != 0; });

    auto windowedHead = m_mementos.head.window_toggle(this->newGltfIndex,
        [this](long index) {
            return this->newGltfIndex.filter([index](int emittedIndex) { return emittedIndex > index; });
        });
    this->headWindowed = windowedHead.publish().ref_count();

    diffs = windowedHead.map([](const auto& head) -> rxcpp::observable<nlohmann::json> {
        return head
            .start_with(nlohmann::json::object())
            .pairwise()
            .map([](const std::tuple<nlohmann::json, nlohmann::json>& sourceTarget) {
                const auto& [source, target] = sourceTarget;
                return reactor::json::minimal_diff(source, target);
            });
    });

    binDiff = m_mementos.head
                  .start_with("{\"glb\": {}, \"files\": {}, \"embedded\": {}}"_json)
                  .pairwise()
                  .map([](const std::tuple<nlohmann::json, nlohmann::json>& sourceTarget) {
                      const auto& [source, target] = sourceTarget;
                      nlohmann::json binSource;
                      binSource["files"] = source["files"];
                      binSource["embedded"] = source["embedded"];
                      binSource["glb"] = source["glb"];
                      nlohmann::json binTarget;
                      binTarget["files"] = target["files"];
                      binTarget["embedded"] = target["embedded"];
                      binTarget["glb"] = target["glb"];
                      auto patch = reactor::json::minimal_diff(binSource, binTarget);
                      return patch;
                  })
                  .filter([](const nlohmann::json& patch) {
                      return !patch.empty();
                  });

    binDiffComb = binDiff.with_latest_from(m_mementos.head);

    uuids
        = diffs.flat_map([](const rxcpp::observable<nlohmann::json>& diff) {
                   return diff.scan(nlohmann::json::object(), [](nlohmann::json uuids, const nlohmann::json& diff) {
                       return reactor::gltf::propertyUUIDs(std::move(uuids), diff, "/gltf");
                   });
               })
              .publish(nlohmann::json())
              .ref_count()
              .filter([](const auto& json) { return !json.is_null(); });

    uuidPaths = uuids
                    .map([](const nlohmann::json& uuids) {
                        nlohmann::json uuidPointers;
                        auto flattened = uuids.at("gltf").flatten();
                        for (const auto& [path, uuid] : flattened.items()) {
                            if (uuid.is_string()) {
                                uuidPointers[uuid.get<std::string>()] = nlohmann::json::json_pointer(path).parent_pointer().to_string();
                            }
                        }
                        return uuidPointers;
                    })
                    .publish(nlohmann::json())
                    .ref_count()
                    .filter([](const auto& json) { return !json.is_null(); });

    head = m_mementos.head
               .zip(uuids)
               .map([](const std::tuple<nlohmann::json, nlohmann::json>& stateUuids) -> nlohmann::json {
                   auto state = std::get<0>(stateUuids);
                   const auto& uuids = std::get<1>(stateUuids);
                   if (uuids.empty()) {
                       state["gltf"] = uuids;
                       return state;
                   }
                   auto flattenedUuids = uuids.flatten();
                   for (const auto& [key, value] : flattenedUuids.items()) {
                       state[nlohmann::json::json_pointer(key)] = value;
                   }
                   return state;
               })
               .map([linearizedSchema](nlohmann::json state) {
                   state["gltf"] = reactor::resolveGltfReferences(linearizedSchema, state["gltf"]);
                   return state;
               })
               .publish(nlohmann::json())
               .ref_count()
               .filter([](const auto& json) { return !json.is_null(); });

    hierarchy = head
                    .map([=](const nlohmann::json& head) -> nlohmann::json {
                        return reactor::resolveGltfHierarchy(filteredSchema, linearizedSchema, head["gltf"]);
                    })
                    .publish(nlohmann::json())
                    .ref_count()
                    .filter([](const auto& json) { return !json.is_null(); });

    resources = head
                    .map([=](const nlohmann::json& head) -> nlohmann::json {
                        return reactor::resolveGltfResources(resourcesCategories, head["gltf"]);
                    })
                    .publish(nlohmann::json())
                    .ref_count()
                    .filter([](const auto& json) { return !json.is_null(); });

    validation = headRaw.observe_on(rxcpp::observe_on_new_thread()).map([](const nlohmann::json& head) {
        if (head.contains("gltf")) {
            return reactor::validation::validate(head["gltf"]);
        }
        return nlohmann::json {};
    });

    headShared = head.map([](const nlohmann::json& head) {
                         return std::make_shared<const nlohmann::json>(head);
                     })
                     .publish(std::make_shared<const nlohmann::json>(nlohmann::json()))
                     .ref_count()
                     .filter([](const auto& json) { return !json->is_null(); });

    operationsByPath = rxcpp::observable<>::just(reactor::operations::byPathPattern(reactor::operations::byName(m_operationsLibrary.schema)));

    std::function<nlohmann::json(const nlohmann::json&)> extractDataStorageIds = [](const nlohmann::json& head) -> nlohmann::json {
        const nlohmann::json& state = head;
        if (!state.contains("/gltf/images"_json_pointer)) {
            return nlohmann::json::object();
        }

        nlohmann::json datastorageIDs = nlohmann::json::object();

        for (auto& [key, value] : state.at("/gltf/images"_json_pointer).items()) {
            const auto& image = value;
            int datastorageID = -1;
            nlohmann::json::json_pointer bufferViewPointer;
            bool bufferViewFound = false;
            if (image.contains("uri")) {
                std::string uri = image.at("uri");
                if (state.at("files").contains(uri)) {
                    datastorageID = state.at("files").at(uri).get<int>();
                }
                if (state.at("embedded").contains(uri)) {
                    datastorageID = state.at("embedded").at(uri).get<int>();
                }
            } else if (image.contains("bufferView")) {
                bufferViewPointer = nlohmann::json::json_pointer {} / "gltf" / nlohmann::json::json_pointer { image.at("bufferView").get<std::string>() };
                auto bufferPointer = nlohmann::json::json_pointer {} / "gltf" / nlohmann::json::json_pointer { state.value<std::string>(bufferViewPointer / "buffer", "-1") };
                if (bufferPointer.back() == "0" && !state.contains(bufferPointer / "uri")) {
                    datastorageID = state.at("/glb/glb_buffer"_json_pointer).get<int>();
                    bufferViewFound = true;
                } else if (bufferPointer.back() != "-1" && state.contains(bufferPointer / "uri")) {
                    std::string uri = state.at(bufferPointer / "uri");
                    datastorageID = state.at("files").at(uri).get<int>();
                    bufferViewFound = true;
                }
            }

            if (datastorageID == -1) {
                continue;
            }

            const nlohmann::json::json_pointer uuidPointer = nlohmann::json::json_pointer {} / "gltf" / "images" / key / "uuid";
            std::string uuid = state.at(uuidPointer);
            nlohmann::json data = nlohmann::json::object();
            if (!bufferViewPointer.empty() && bufferViewFound) {
                data["bufferview"] = bufferViewPointer.to_string();
            }
            data["datastorage"] = datastorageID;
            datastorageIDs[uuid] = data;
        }

        return datastorageIDs;
    };

    imageUUIDToDatastorageID = head.map(extractDataStorageIds).start_with(nlohmann::json::object());

    imageDiff = imageUUIDToDatastorageID
                    .pairwise()
                    .map([](const std::tuple<nlohmann::json, nlohmann::json>& sourceTarget) {
                        const auto& [source, target] = sourceTarget;
                        auto patch = reactor::json::minimal_diff(source, target);
                        return patch;
                    })
                    .filter([](const nlohmann::json& patch) {
                        return !patch.empty();
                    });

    std::function<nlohmann::json(const std::tuple<nlohmann::json, nlohmann::json>&)> imageDiffToImageMetadata = [&](const std::tuple<nlohmann::json, nlohmann::json>& tuple) -> nlohmann::json {
        const nlohmann::json diff = std::get<0>(tuple);
        const nlohmann::json state = std::get<1>(tuple);
        nlohmann::json imageMetadata = nlohmann::json::object();
        for (auto& change : diff) {
            // path starts with a slash, so we remove it
            std::string uuid = change.at("path");
            uuid = uuid.erase(0, 1);
            // if the change only affects the datastorage or bufferview, the change only contains that part
            // in that case the path looks like this: /uuid/datastorage or /uuid/bufferview
            // (the first slash is removed above)
            // since we only want the uuid we remove everything after the second slash as well
            uuid = uuid.substr(0, uuid.find("/"));

            if (change.at("op") == "remove") {
                // specifically set this to "removed" to indicate that the image has been removed
                // this has to be done cause values without changes are not included in the diff
                imageMetadata[uuid] = "removed";
                continue;
            } else {
                try {
                    const nlohmann::json changeValue = change.at("value");
                    const std::string changePath = change.at("path");

                    // if the change is only in a subpart (datastorage or bufferview) we have to handle that differently
                    // thats why we have this if here where we check the change path
                    if (changeValue.contains("bufferview") || changePath.ends_with("bufferView")) {
                        // extract bufferview data from datastorage
                        nlohmann::json::json_pointer bufferViewPointer;
                        if (changeValue.contains("bufferview")) {
                            bufferViewPointer = nlohmann::json::json_pointer { changeValue.at("bufferview").get<std::string>() };
                        } else {
                            bufferViewPointer = nlohmann::json::json_pointer {} / "gltf" / "bufferViews" / changeValue.get<int>();
                        }
                        int datastorageID = changeValue.at("datastorage").get<int>();
                        auto& bufferView = state[bufferViewPointer];

                        uint32_t byteLength = bufferView["byteLength"];
                        uint32_t byteOffset = 0u;
                        if (bufferView.contains("byteOffset")) {
                            byteOffset = bufferView["byteOffset"];
                        }

                        const std::vector<uint8_t>& bufferData = m_dataStorage.at(datastorageID);
                        if (bufferData.size() < byteOffset + byteLength) {
                            throw std::runtime_error("Buffer data invalid");
                        }
                        std::vector<uint8_t> data(byteLength);
                        data.assign(bufferData.begin() + byteOffset, bufferData.begin() + byteOffset + byteLength);
                        imageMetadata[uuid] = reactor::image::decodeInfo(data);
                    } else {
                        if (changeValue.contains("datastorage") || changePath.ends_with("datastorage")) {
                            int datastorageID;
                            if (changePath.ends_with("datastorage")) {
                                datastorageID = changeValue.get<int>();
                            } else {
                                datastorageID = changeValue.at("datastorage").get<int>();
                            }
                            const auto& imageData = m_dataStorage.at(datastorageID);
                            imageMetadata[uuid] = reactor::image::decodeInfo(imageData);
                        }
                    }

                } catch (...) {
                }
            }
        }

        return imageMetadata;
    };

    std::function<nlohmann::json(nlohmann::json, const nlohmann::json&)> combineImageMetadata = [](nlohmann::json seed, const nlohmann::json& newValue) -> nlohmann::json {
        // combine source and target
        for (const auto& item : newValue.items()) {
            const std::string key = item.key();
            const nlohmann::json& value = item.value();
            if (value.is_string() && value == "removed") {
                seed.erase(key);
            } else {
                seed[key] = value;
            }
        }

        return seed;
    };

    imageMetadata = imageDiff
                        .observe_on(rxcpp::observe_on_new_thread())
                        .with_latest_from(head)
                        .map(imageDiffToImageMetadata)
                        .scan(nlohmann::json::object(), combineImageMetadata)
                        .map([](const nlohmann::json& imageMetadata) {
                            return imageMetadata;
                        });
}

DispatchQueue::ResultPipes Gltf::schedule(const nlohmann::json& task)
{
    return m_operationsDispatchQueue.launch([task, this]() {
        std::lock_guard lock(m_mementosMutex);
        auto emptyState = nlohmann::json::object({ { "files", nlohmann::json::object() },
            { "embedded", nlohmann::json::object() },
            { "glb", nlohmann::json::object() },
            { "gltf", nlohmann::json::object() } });
        auto head = m_mementos.present.past.empty() ? emptyState : *m_mementos.present.past.back();
        auto pastHead = head;
        std::string successMessage = "";
        std::function<void(const nlohmann::json&)> callback = [&successMessage](const nlohmann::json& info) {
            if (info.contains("message")) {
                successMessage = info["message"];
            }
        };

        std::string operationName = task.begin().key();

        // handle dangerous.combineOperations seperately cause it has a different structure
        if (operationName == "dangerous.combineOperations") {
            const auto operations = task["dangerous.combineOperations"];
            if (operations.is_array() && operations.size() > 0) {
                for (const auto& arrayElem : operations) {
                    if (!arrayElem.is_object()) {
                        continue;
                    }
                    const auto operation = arrayElem[arrayElem.begin().key()];
                    if (operation.contains("operation")) {
                        std::string tmpName = operation["operation"];
                        operationName = tmpName + " (combineOperations)";
                    }
                }
            }
        } else {
            // handle all other cases
            if (operationName == "dangerous.forEach" || operationName == "dangerous.forEachElement") {
                const auto operation = task[operationName];
                if (operation.contains("operation")) {
                    std::string tmpName = operation["operation"];
                    operationName = tmpName + " (forEach)";
                }
            }
        }

        head = m_operationsLibrary.call(task, std::move(head), m_dataStorage, callback);
        auto diff = nlohmann::json::diff(pastHead, head);
        if (!diff.empty()) {
            m_mementos.commit(std::move(head));
        }
        return successMessage;
    });
}

void Gltf::loadPlugin(const std::vector<operations::Operation>& pluginOperations)
{
    m_operationsLibrary.loadPlugin(pluginOperations);
}

void Gltf::apply(nlohmann::json&& gltfState)
{
    std::lock_guard lock(m_mementosMutex);
    m_mementos.commit(std::move(gltfState));
}

void Gltf::clearHistory()
{
    std::lock_guard lock(m_mementosMutex);
    m_mementos.clear();
    m_dataStorage.clear();
}

void Gltf::undo()
{
    std::lock_guard lock(m_mementosMutex);
    m_mementos.moveBack();
}

void Gltf::redo()
{
    std::lock_guard lock(m_mementosMutex);
    m_mementos.moveForward();
}
