// Copyright © 2022 UX3D GmbH. All rights reserved.

#include "EditOperations.h"
#include "HelperFunctions.h"

#include "HelperFunctions.h"
#include "datastorage/DataStorage.h"
#include "operations/Library.h"
#include "operations/Operations.h"
#include "operations/Schema.h"
#include "reactor/ImageOperations.h"
#include "reactor/Reactor.h"
#include "reactor/Schema.h"
#include "reactor/Utils.h"

#include <glm.hpp>

// debug:
#include <iostream>

#include <gltf_schema/gltf_single_json.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>

#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <fmt/format.h>

using namespace ux3d;
using namespace ux3d::operations;
using namespace std::string_literals;

nlohmann::json mergeStates(nlohmann::json&& state, nlohmann::json&& importState, datastorage::DataStorage& datastorage, const nlohmann::json& uriProperties, const nlohmann::json& childOfRoots, const nlohmann::json& backwardReferences)
{
    // convert glb_buffer of imported file
    if (importState.contains("glb") && importState.at("glb").size() > 0) {
        auto datastorageID = importState.at("glb").at("glb_buffer").get<uint64_t>();
        for (auto& buffer : importState["gltf"]["buffers"]) {
            if (!buffer.contains("uri") || buffer.at("uri").get<std::string>().size() == 0) {
                importState.erase(reactor::utils::getDataIDPath(buffer, state)); // cleanup old entry
                // We need an uri that does not collide in current and imported state
                buffer["uri"] = reactor::utils::getRandomString() + ".bin";
                importState[reactor::utils::getDataIDPath(buffer, state)] = datastorageID;
            }
        }
    }

    // Check for URI collisions and create new URI if necessary
    for (auto& [key, value] : uriProperties.items()) {
        auto path = nlohmann::json::json_pointer(key).parent_pointer();
        if (!importState["gltf"].contains(path)) {
            continue;
        }
        for (auto& property : importState["gltf"].at(path)) {
            if (!property.contains("uri")) {
                continue;
            }
            std::string uri = property.at("uri").get<std::string>();
            if (reactor::utils::isURIUsed(uri, state)) { // Check for collisions in main state
                // fetch datastorage ID before we delete entry
                auto dataID = importState.at(reactor::utils::getDataIDPath(property, state)).get<uint64_t>();
                importState.erase(reactor::utils::getDataIDPath(property, state));

                // create new unique identifier
                std::string newUri = reactor::utils::getRandomString() + uri;

                // Update uri in importstate and property
                property["uri"] = newUri;
                importState[reactor::utils::getDataIDPath(property, state)] = dataID;
            }
        }
    }

    // Now all uri collisions should be resolved. We can now merge the datastorage identifier
    if (importState.contains("files") && importState.at("files").is_object()) {
        if (!state.contains("files")) {
            state["files"] = nlohmann::json::object();
        }
        state["files"].merge_patch(importState["files"]);
    }

    if (importState.contains("embedded") && importState.at("embedded").is_object()) {
        if (!state.contains("embedded")) {
            state["embedded"] = nlohmann::json::object();
        }
        state["embedded"].merge_patch(importState["embedded"]);
    }

    // Return all paths into the glTF that have a reference to the property given by propertyPath
    auto getBackwardReferenceProperties = [&](auto& gltf, auto propertyPath) -> std::vector<nlohmann::json::json_pointer> {
        std::vector<nlohmann::json::json_pointer> propertyPaths;
        uint64_t targetIndex = std::stoi(propertyPath.back());
        auto pattern = reactor::schema::mapToPattern(childOfRoots, propertyPath);
        if (backwardReferences.contains(pattern)) {
            for (const auto& referencePattern : backwardReferences[pattern]) {
                reactor::schema::patternForEach(referencePattern, gltf, [&](const nlohmann::json::json_pointer& pointer) {
                    if (gltf.at(pointer).template get<uint64_t>() == targetIndex) {
                        propertyPaths.push_back(pointer);
                    }
                });
            }
        }
        return propertyPaths;
    };

    // Fix all indices of imported glTF with calculated offsets
    for (auto [childOfRootPattern, value] : childOfRoots.items()) {

        auto originalImportGltf = importState.at("gltf");
        reactor::schema::patternForEach(childOfRootPattern, importState.at("gltf"), [&](const nlohmann::json::json_pointer& pointer) {
            uint32_t indexOffset = 0;
            if (state["gltf"].contains(pointer.parent_pointer())) {
                indexOffset = state["gltf"].at(pointer.parent_pointer()).size();
            }
            // use backup of imoportGltf as we change the indices already
            auto backwardReferenceProperties = getBackwardReferenceProperties(originalImportGltf, pointer);

            for (auto gltfPath : backwardReferenceProperties) {
                importState.at("gltf").at(gltfPath) = importState.at("gltf").at(gltfPath).get<uint64_t>() + indexOffset;
            }
        });
    }

    for (auto& [key, value] : childOfRoots.items()) {
        // get the location of the property category
        // "/accessors/\\d+" ->  "/accessors"
        auto path = nlohmann::json::json_pointer(key).parent_pointer();

        if (importState["gltf"].contains(path) && importState["gltf"].at(path).is_array()) {
            // If importState contains elements, we add them to our main glTF (state)
            if (!state.at("gltf").contains(path)) {
                state.at("gltf")[path] = nlohmann::json::array();
            }
            state.at("gltf")[path].insert(state.at("gltf")[path].end(), importState["gltf"].at(path).begin(), importState["gltf"].at(path).end());
        }
    }

    // Merge extensionUsed/Required
    nlohmann::json::json_pointer extensionUsedPointer("/gltf/extensionsUsed");
    if (importState.contains(extensionUsedPointer)) {
        if (state.contains(extensionUsedPointer)) {
            std::set<std::string> extensionsUsed(state.at(extensionUsedPointer).begin(), state.at(extensionUsedPointer).end());
            extensionsUsed.insert(importState.at(extensionUsedPointer).begin(), importState.at(extensionUsedPointer).end());
            state.at(extensionUsedPointer) = extensionsUsed;
        } else {
            state[extensionUsedPointer] = importState[extensionUsedPointer];
        }
    }

    nlohmann::json::json_pointer extensionRequiredPointer("/gltf/extensionsRequired");
    if (importState.contains(extensionRequiredPointer)) {
        if (state.contains(extensionRequiredPointer)) {
            std::set<std::string> extensionsRequired(state.at(extensionRequiredPointer).begin(), state.at(extensionRequiredPointer).end());
            extensionsRequired.insert(importState.at(extensionRequiredPointer).begin(), importState.at(extensionRequiredPointer).end());
            state.at(extensionRequiredPointer) = extensionsRequired;
        } else {
            state[extensionRequiredPointer] = importState[extensionRequiredPointer];
        }
    }

    return state;
}

Operation edit::mergeGltf(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    auto childOfRoots = reactor::gltf::schema::filterChildOfRootProperties(linearizedGltfSchema);
    auto uriProperties = reactor::gltf::schema::filterUriProperties(linearizedGltfSchema);
    auto backwardReferences = reactor::gltf::schema::backwardReferences(linearizedGltfSchema);

    Operation operation;
    operation.name = "merge_gltf";

    operation.schema = schema::operation(
        "Merge glTF",
        "A second glTF referenced by a file path is merged into the current one",
        schema::object({ { "filepath", schema::string() } }));

    operation.operation = [childOfRoots, uriProperties, backwardReferences](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json importState = nlohmann::json::object();

        // Load glTF that we want to import into its own state
        auto loadTaskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        loadTaskDescription["load_file"]["filepath"] = params["filepath"];
        importState = library.call(std::move(loadTaskDescription), std::move(importState), datastorage, callback);

        state = mergeStates(std::move(state), std::move(importState), datastorage, uriProperties, childOfRoots, backwardReferences);
        importState = nlohmann::json::object(); // importState was moved

        return state;
    };

    return operation;
}

Operation edit::importAsset(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    auto childOfRoots = reactor::gltf::schema::filterChildOfRootProperties(linearizedGltfSchema);
    auto uriProperties = reactor::gltf::schema::filterUriProperties(linearizedGltfSchema);
    auto backwardReferences = reactor::gltf::schema::backwardReferences(linearizedGltfSchema);

    Operation operation;
    operation.name = "import_asset";

    operation.schema = schema::operation(
        "Import 3D Asset",
        "Import a 3D Asset (*.gltf *.glb *.blend *.3ds *.dxf *.fbx *.gltf *.obj *.ply *.stl *.x3d)",
        schema::object({ { "filepath", schema::string() } }));

    operation.operation = [childOfRoots, uriProperties, backwardReferences](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::filesystem::path importFile(params["filepath"].get<std::string>());

        if (std::filesystem::exists(importFile) == false) {
            throw std::runtime_error("file not found: " + importFile.string());
        }

        if (importFile.extension() == ".gltf" || importFile.extension() == ".glb" || importFile.extension() == ".gestalt") {
            nlohmann::json task;
            task["merge_gltf"]["filepath"] = importFile.string();
            state = library.call(std::move(task), std::move(state), datastorage, callback);
            { // resolve KHR_draco_mesh_compression
                auto taskDescription = nlohmann::json({ { "resolve_draco_for_each", nlohmann::json::object() } });
                taskDescription["resolve_draco_for_each"]["deleteEncodedData"] = false;
                taskDescription["resolve_draco_for_each"]["cleanExtensionsUsed"] = false;
                state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
            }
            return state;
        }

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(importFile.string(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_FindInvalidData);
        if (scene == nullptr) {
            std::string reason = importer.GetErrorString();
            throw std::runtime_error("Unable to import asset: " + reason);
        }

        std::filesystem::path tempPath = std::filesystem::temp_directory_path();

        std::filesystem::remove_all(tempPath / "assimp");
        std::filesystem::create_directory(tempPath / "assimp");
        std::filesystem::path exportPath = tempPath / "assimp" / (importFile.stem().string() + ".gltf");

        Assimp::Exporter exporter;
        auto result = exporter.Export(scene, "gltf2", exportPath.string());

        if (result == aiReturn_FAILURE) {
            throw std::runtime_error("Import failed");
        }

        nlohmann::json importState = nlohmann::json::object();
        { // load glTF that was exported by assimp into its own state
            auto loadTaskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
            loadTaskDescription["load_file"]["filepath"] = exportPath.string();
            loadTaskDescription["load_file"]["resourceDirectory"] = importFile.parent_path().string();
            importState = library.call(std::move(loadTaskDescription), std::move(importState), datastorage, callback);
        }
        { // clean unknown extensions
            auto taskDescription = nlohmann::json({ { "remove_unknown_extension", nlohmann::json::object() } });
            importState = library.call(std::move(taskDescription), std::move(importState), datastorage, callback);
        }
        { // clean KHR_materials_pbrSpecularGlossiness
            auto taskDescription = nlohmann::json({ { "remove_extension", nlohmann::json::object() } });
            taskDescription["remove_extension"]["property"] = "KHR_materials_pbrSpecularGlossiness";
            importState = library.call(std::move(taskDescription), std::move(importState), datastorage, callback);
        }

        state = mergeStates(std::move(state), std::move(importState), datastorage, uriProperties, childOfRoots, backwardReferences);
        importState = nlohmann::json::object(); // importState was moved

        // cleanup
        std::filesystem::remove_all(tempPath / "assimp");

        return state;
    };
    return operation;
}

Operation edit::blendImages(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "blend_images";

    operation.schema = schema::operation(
        "Blend Images",
        "Blend two images together by a given weight value resulting in a third image",
        schema::object({ { "imageA", schema::pointer("/gltf/images/\\d+") }, { "imageB", schema::pointer("/gltf/images/\\d+") }, { "weight", schema::number(0.5) } }));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto pathA = nlohmann::json::json_pointer(params["imageA"].get<std::string>());
        auto pathB = nlohmann::json::json_pointer(params["imageB"].get<std::string>());
        if (!state.contains(pathA) || !state.contains(pathB)) {
            throw std::invalid_argument("Image not found");
        }

        std::vector<uint8_t> rawDataA = reactor::utils::getImageData(pathA, state, datastorage);
        auto imagedataA = reactor::image::decode(rawDataA);
        std::vector<uint8_t> rawDataB = reactor::utils::getImageData(pathB, state, datastorage);
        auto imagedataB = reactor::image::decode(rawDataB);
        uint32_t maxWidth = std::max(imagedataA.info["width"].get<uint32_t>(), imagedataB.info["width"].get<uint32_t>());
        uint32_t maxHeight = std::max(imagedataA.info["height"].get<uint32_t>(), imagedataB.info["height"].get<uint32_t>());

        imagedataA = reactor::image::scaleImage(imagedataA, maxWidth, maxHeight);
        imagedataB = reactor::image::scaleImage(imagedataB, maxWidth, maxHeight);
        float weight = params["weight"].get<float>();
        if (weight < 0.0f) {
            weight = 0.0f;
        }
        if (weight > 1.0f) {
            weight = 1.0f;
        }
        auto imagedataC = reactor::image::blendImages(imagedataA, imagedataB, weight);

        auto imageC = nlohmann::json::object();
        imageC["uri"] = reactor::utils::createURIStem(state) + ".png";
        state["gltf"]["images"].emplace_back(imageC);

        nlohmann::json encoding = { { "mimeType", "image/png" } };
        auto rawData = reactor::image::encode(imagedataC, encoding);
        auto dataIDPath = reactor::utils::getDataIDPath(imageC, state);
        datastorage::DataStorage::ID dataStorageID = datastorage.insert(rawData);
        state[dataIDPath] = dataStorageID;
        return state;
    };

    return operation;
}

Operation edit::applyNodeTransform(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "apply_node_transform";

    operation.schema = schema::operation(
        "Apply Node Transform to Mesh",
        "Apply the current transformation of the node directly to the vertex data of the assigned mesh",
        schema::object({ { "path", schema::pointer("/gltf/nodes/\\d+") } }));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());
        nlohmann::json::json_pointer pathWithoutGltf = nlohmann::json::json_pointer(params["path"].get<std::string>().substr(5));
        if (!state.contains(path)) {
            throw std::invalid_argument("Node not found");
        }

        if (!state.contains(path / "mesh")) {
            throw std::runtime_error("Node has no mesh assigned");
        }

        if (state.contains(path / "children") && !state.at(path / "children").empty()) {
            throw std::runtime_error("Node transform can not be applied to mesh data, because the node has children.");
        }

        if (state.contains(path / "matrix")) {
            auto taskDescription = nlohmann::json({ { "change_transformation_format", nlohmann::json::object() } });
            taskDescription["change_transformation_format"]["path"] = path.to_string();
            taskDescription["change_transformation_format"]["storageType"] = "TRS";
            state = library.call(taskDescription, std::move(state), datastorage, callback);
        }

        bool isIdentity = true;
        auto& vectorSchemaProperties = linearizedGltfSchema.at("/nodes/\\d+").at("properties");

        glm::vec3 translation = vectorSchemaProperties.at("/translation/default"_json_pointer);
        glm::vec3 scale = vectorSchemaProperties.at("/scale/default"_json_pointer);
        glm::dquat rotation = vectorSchemaProperties.at("/rotation/default"_json_pointer);

        if (state.contains(path / "translation") && state.at(path / "translation") != vectorSchemaProperties.at("/translation/default"_json_pointer)) {
            isIdentity = false;
        } else if (state.contains(path / "rotation") && state.at(path / "rotation") != vectorSchemaProperties.at("/rotation/default"_json_pointer)) {
            isIdentity = false;
        } else if (state.contains(path / "scale") && state.at(path / "scale") != vectorSchemaProperties.at("/scale/default"_json_pointer)) {
            isIdentity = false;
        }

        if (isIdentity) {
            throw std::runtime_error("Node transform is identity.");
        }

        auto pointerArrayNode = reactor::getBackwardReferenceProperties(state.at("gltf"), pathWithoutGltf, linearizedGltfSchema);
        for (const auto& pointer : pointerArrayNode) {
            if (!pointer.parent_pointer().empty()) {
                if (pointer.back() == "node" && pointer.parent_pointer().back() == "target") {
                    throw std::runtime_error("Node transform can not be applied to mesh data, because the node is part of an animation.");
                }
                if (pointer.parent_pointer().back() == "joints") {
                    throw std::runtime_error("Node transform can not be applied to mesh data, because the node is a joint of a skin.");
                }
            }
        }
        nlohmann::json::json_pointer meshPointer = nlohmann::json::json_pointer("/meshes/" + std::to_string(state.at(path / "mesh").get<int>()));
        auto pointerArrayMesh = reactor::getBackwardReferenceProperties(state.at("gltf"), meshPointer, linearizedGltfSchema);

        for (const auto& pointer : pointerArrayMesh) {
            if (!pointer.parent_pointer().parent_pointer().empty() && pointer.parent_pointer().parent_pointer().back() == "nodes" && pointer != pathWithoutGltf / "mesh") {
                throw std::runtime_error("Transform can not be applied to mesh data, because mesh is referenced by multiple nodes.");
            }
        }

        if (state.contains(path / "translation")) {
            translation = state.at(path / "translation");
        }
        if (state.contains(path / "scale")) {
            scale = state.at(path / "scale");
        }
        if (state.contains(path / "rotation")) {
            rotation = state.at(path / "rotation");
        }

        // collect all used bufferViews that can be deleted afterwards
        auto usedBufferViews = nlohmann::json::array();

        // Create a copy of the primitiveArray. Otherwise the iterator in the for loop is invalidated by the operation called inside
        const nlohmann::json primitiveArray = state.at("/gltf"_json_pointer / meshPointer / "primitives");

        for (const auto& primitive : primitiveArray) {
            for (const auto& [attributeName, attributeValue] : primitive.at("attributes").items()) {
                if (attributeName == "POSITION" || attributeName == "NORMAL" || attributeName == "TANGENT") {
                    nlohmann::json::json_pointer accessorPathWithoutGltf("/accessors/" + std::to_string(attributeValue.get<int>()));
                    nlohmann::json::json_pointer accessorPath("/gltf/accessors/" + std::to_string(attributeValue.get<int>()));
                    auto& accessor = state.at(accessorPath);
                    if (accessor.contains("normalized") && accessor.at("normalized").get<bool>()) {
                        throw std::runtime_error("Transform cannot be applied because Accessor holds normalized data.");
                    }
                    if (accessor.contains("sparse")) {
                        throw std::runtime_error("Transform cannot be applied because Accessor is sparse.");
                    }
                    auto pointerArrayAccessor = reactor::getBackwardReferenceProperties(state.at("gltf"), accessorPathWithoutGltf, linearizedGltfSchema);
                    if (pointerArrayAccessor.size() > 1) {
                        throw std::runtime_error("Transform cannot be applied because Accessor is referenced by multiple objects.");
                    }

                    auto accessorData = reactor::utils::getAccessorByteDataPacked(accessorPath, state, datastorage);

                    auto componentType = accessor.at("componentType").get<reactor::gltf::ComponentType>();
                    auto attributeType = accessor.at("type").get<reactor::gltf::Type>();

                    if (componentType != reactor::gltf::ComponentType::Float && attributeType != reactor::gltf::Type::VEC4 && attributeType != reactor::gltf::Type::VEC3) {
                        throw std::invalid_argument("Unsupported type");
                    }

                    if (attributeType == reactor::gltf::Type::VEC3) {
                        std::vector<glm::vec3> vecData;
                        vecData.resize(accessorData.size() / sizeof(glm::vec3));
                        std::memcpy(vecData.data(), accessorData.data(), accessorData.size());
                        for (auto& vec : vecData) {
                            glm::mat4x4 transform(1.0); // = Identity Matrix

                            transform = glm::translate(transform, translation);

                            glm::mat4x4 rotationMat = glm::mat4_cast(rotation);
                            transform = transform * rotationMat;

                            transform = glm::scale(transform, scale);

                            if (attributeName == "NORMAL") {
                                transform = glm::transpose(glm::inverse(transform));
                            }

                            vec = transform * glm::vec4(vec, 1.0);

                            if (attributeName == "NORMAL") {
                                vec = glm::normalize(vec);
                            }
                        }
                        std::memcpy(accessorData.data(), vecData.data(), accessorData.size());

                    } else if (attributeType == reactor::gltf::Type::VEC4) {
                        std::vector<glm::vec4> vecData;
                        vecData.resize(accessorData.size() / sizeof(glm::vec4));
                        std::memcpy(vecData.data(), accessorData.data(), accessorData.size());
                        for (auto& vec : vecData) {
                            glm::vec3 tmp(vec);
                            glm::mat4x4 transform(1.0); // = Identity Matrix

                            transform = glm::translate(transform, translation);

                            glm::mat4x4 rotationMat = glm::mat4_cast(rotation);
                            transform = transform * rotationMat;

                            transform = glm::scale(transform, scale);

                            tmp = transform * glm::vec4(tmp, 0.0);
                            tmp = glm::normalize(tmp);
                            vec = glm::vec4(tmp, vec.w);
                        }
                        std::memcpy(accessorData.data(), vecData.data(), accessorData.size());
                    }
                    auto bufferViewPath = nlohmann::json::json_pointer("/gltf/bufferViews") / accessor["bufferView"].get<uint64_t>();
                    usedBufferViews.push_back(bufferViewPath.to_string());

                    auto bufferPath = nlohmann::json::json_pointer("/gltf/buffers") / state.at(bufferViewPath)["buffer"].get<uint64_t>();

                    auto newBufferView = reactor::utils::appendBufferView(bufferPath, accessorData, state, datastorage);
                    accessor["bufferView"] = std::stoi(newBufferView.back());
                    accessor["byteOffset"] = 0;

                    {
                        auto taskDescription = nlohmann::json({ { "calculate_min_max", nlohmann::json::object() } });
                        taskDescription["calculate_min_max"]["path"] = accessorPath.to_string();
                        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
                    }

                } else if (attributeName.starts_with("JOINTS") || attributeName.starts_with("WEIGHTS")) {
                    throw std::runtime_error("Transform cannot be applied because Primitive contains Skin data.");
                }
            }
        }

        auto taskDescription = nlohmann::json({ { "reset", nlohmann::json::object() } });

        taskDescription["reset"]["path"] = (path / "translation").to_string();
        state = library.call(taskDescription, std::move(state), datastorage, callback);
        taskDescription["reset"]["path"] = (path / "rotation").to_string();
        state = library.call(taskDescription, std::move(state), datastorage, callback);
        taskDescription["reset"]["path"] = (path / "scale").to_string();
        state = library.call(taskDescription, std::move(state), datastorage, callback);

        { // Remove no longer used bufferViews
            std::string op = "delete_bufferview";
            auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
            taskDescription[op]["path"] = usedBufferViews;
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        return state;
    };

    return operation;
}

/**
 * @brief This function is only here due to an import bug caused by windows.h
 * assimp can't be used in FileOperations.cpp.
 * The bug: https://github.com/assimp/assimp/issues/2271
 * the fix: https://github.com/assimp/assimp/pull/4846
 * If you find this function and assimp has been updated to a version newer than 5.3.0 you can move this code back into
 * the load operation in FileOperations.cpp.
 *
 */
Operation edit::load()
{
    Operation operation;
    operation.name = "load_file";

    // fill the operation schema header
    operation.schema = schema::operation(
        "Load glTF, GLB or other model File",
        "Load a glTF, GLB or other model file from a filesystem path. The supported file types are *.gltf *.glb *.blend *.3ds *.dxf *.fbx *.gltf *.obj *.ply *.stl *.x3d",
        schema::object({ { "filepath", schema::string() } }, { { "resourceDirectory", schema::string() } }));

    // define the operation function
    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::filesystem::path filepath(params["filepath"].get<std::string>());
        filepath = filepath.make_preferred();
        if (!std::filesystem::exists(filepath)) {
            throw std::invalid_argument("File does not exist: " + filepath.string());
        }

        std::string extension = filepath.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
        if (extension.compare(".gltf") == 0) {
            return file::loadGltf(params, std::move(state), datastorage, library);
        } else if (extension.compare(".glb") == 0 || extension.compare(".gestalt") == 0) {
            return file::loadGlb(params, std::move(state), datastorage, library);
        } else {
            // Convert the other asset file to glTF using Assimp
            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(filepath.string(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_FindInvalidData);
            if (scene == nullptr) {
                std::string reason = importer.GetErrorString();
                throw std::runtime_error("Unable to import asset: " + reason);
            }

            std::filesystem::path tempPath = std::filesystem::temp_directory_path();

            std::filesystem::remove_all(tempPath / "assimp");
            std::filesystem::create_directory(tempPath / "assimp");
            std::filesystem::path exportPath = tempPath / "assimp" / (filepath.stem().string() + ".gltf");

            Assimp::Exporter exporter;
            auto result = exporter.Export(scene, "gltf2", exportPath.string());

            if (result == aiReturn_FAILURE) {
                throw std::runtime_error("Import failed");
            }

            nlohmann::json importState = nlohmann::json::object();
            { // load glTF that was exported by assimp into its own state
                auto loadTaskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
                loadTaskDescription["load_file"]["filepath"] = exportPath.string();
                loadTaskDescription["load_file"]["resourceDirectory"] = filepath.parent_path().string();
                importState = library.call(std::move(loadTaskDescription), std::move(importState), datastorage, callback);
            }
            { // clean unknown extensions
                auto taskDescription = nlohmann::json({ { "remove_unknown_extension", nlohmann::json::object() } });
                importState = library.call(std::move(taskDescription), std::move(importState), datastorage, callback);
            }
            { // clean KHR_materials_pbrSpecularGlossiness
                auto taskDescription = nlohmann::json({ { "remove_extension", nlohmann::json::object() } });
                taskDescription["remove_extension"]["property"] = "KHR_materials_pbrSpecularGlossiness";
                importState = library.call(std::move(taskDescription), std::move(importState), datastorage, callback);
            }

            // cleanup
            std::filesystem::remove_all(tempPath / "assimp");

            importState["glb"] = nlohmann::json::object();
            importState["embedded"] = nlohmann::json::object();

            return importState;
        }
    };

    return operation;
}
