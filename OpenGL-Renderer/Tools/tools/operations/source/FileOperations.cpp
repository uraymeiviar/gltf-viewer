// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "FileOperations.h"

#include "HelperFunctions.h"
#include "PropertyOperations.h"

#include "filesystem/File.h"
#include "filesystem/FileSystem.h"
#include "operations/Library.h"
#include "operations/Schema.h"
#include "reactor/Utils.h"

#include "nlohmann/json.hpp"

#include "subprocess.hpp"

#include <filesystem>
#include <fstream>

#include "core/Platform.h"

#ifdef UX3D_IS_WINDOWS
#include <WinNT.h>
#include <libloaderapi.h>
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

#ifdef UX3D_IS_APPLE
#include <limits.h>
#include <mach-o/dyld.h>
#endif

#ifdef UX3D_IS_LINUX
#include <linux/limits.h>
#include <unistd.h>
#endif

using namespace ux3d;
using namespace ux3d::operations;

Operation file::save()
{
    Operation operation;
    operation.name = "save_file";

    // fill the operation schema header
    operation.schema = schema::operation(
        "Save as glTF or GLB File",
        "Save the current glTF as a .gltf or .glb file",
        schema::object(
            { { "filepath", schema::string(".*\\.gltf|.*\\.glb") } },
            { { "overwrite", schema::boolean() },
                { "cleanupData", schema::boolean() } }));

    // define the operation function
    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (!state.contains("gltf")) {
            throw std::invalid_argument("No glTF given");
        }

        auto stateCopy = state;

        gltf::removePropertiesFromGltf(state.at("gltf"), { { "property", "ux3d_gestaltor" } }, true);

        {
            // for rendering, we store draco data in compressed and uncompressed form.
            // when saving, we only want to keep compressed draco
            auto taskDescription = nlohmann::json({ { "remove_redundant_draco", nlohmann::json::object() } });
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        if (params.contains("cleanupData") && params.at("cleanupData").get<bool>() == true) {
            // remove unused bufferViews
            if (state["gltf"].contains("bufferViews")) {
                auto params = nlohmann::json::object();
                params["skipIfReferenced"] = true;
                params["path"] = nlohmann::json::array();
                for (int32_t i = 0; i < state["gltf"]["bufferViews"].size(); i++) {
                    params["path"].push_back("/gltf/bufferViews/" + std::to_string(i));
                }
                auto taskDescription = nlohmann::json({ { "delete_bufferview", params } });
                state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
            }
            // remove unused data from buffers and merge
            if (state["gltf"].contains("buffers")) {
                int32_t bufferCount = state["gltf"]["buffers"].size();
                for (int32_t i = bufferCount - 1; i >= 0; i--) {
                    // trim in reverse order as they can be removed when empty
                    auto taskDescription = nlohmann::json({ { "trim_buffer", nlohmann::json::object() } });
                    taskDescription["trim_buffer"]["path"] = "/gltf/buffers/" + std::to_string(i);
                    state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
                }
                if (state["gltf"]["buffers"].size() > 1) {
                    auto taskDescription = nlohmann::json({ { "merge_buffers", nlohmann::json::object() } });
                    taskDescription["merge_buffers"]["bufferPaths"] = nlohmann::json::array();
                    // Add all buffers
                    for (int32_t i = 0; i < state["gltf"]["buffers"].size(); i++) {
                        taskDescription["merge_buffers"]["bufferPaths"].push_back("/gltf/buffers/" + std::to_string(i));
                    }
                    state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
                }
            }
        }

        std::filesystem::path filepath(params["filepath"].get<std::string>());
        std::string extension = filepath.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
        nlohmann::json result;
        if (extension.compare(".gltf") == 0) {
            result = saveGltf(params, std::move(state), datastorage, library);
        } else if (extension.compare(".glb") == 0) {
            result = saveGlb(params, std::move(state), datastorage, library, callback);
        } else {
            throw std::invalid_argument("Invalid file extension");
        }

        if (!result.empty()) {
            throw std::runtime_error(result.dump(4));
        }

        return stateCopy;
    };

    return operation;
}

Operation file::saveSelection(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "save_properties";

    nlohmann::json childOfRootPaths = R"(
    {
        "type": "array",
        "items": {
            "type": "string",
            "format": "json-pointer",
            "pattern": "^.*$"
        },
        "uniqueItems": true,
        "minItems": 1
    }
    )"_json;

    // fill the operation schema header
    operation.schema = schema::operation(
        "Save properties as glTF or GLB File",
        "Save specified properties as a .gltf or .glb file",
        schema::object({ { "filepath", schema::string(".*\\.gltf|.*\\.glb") }, { "paths", childOfRootPaths } }, { { "overwrite", schema::boolean() } }));

    // define the operation
    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        // mark all objects present in the considered subtrees
        nlohmann::json stateWithSubtreeInfo = state;
        for (const std::string& path : params["paths"]) {
            if (!state.contains(nlohmann::json::json_pointer(path))) {
                throw std::invalid_argument("Property not found in glTF: " + path);
            }
            nlohmann::json::json_pointer pointer(path.substr(5)); // omit the "/gltf" part in the front
            markSubtreeObjects(linearizedGltfSchema, stateWithSubtreeInfo["gltf"], pointer, true);
        }

        // remove all non-subtree objects
        nlohmann::json reducedState = reduceGltfToSubtreeObjects(std::move(stateWithSubtreeInfo), datastorage, linearizedGltfSchema, library, callback);

        // only keep the binary data that is actually needed by the still present objects
        datastorage::DataStorage reducedDatastorage = datastorage;

        if (reducedState["gltf"].contains("buffers") && reducedState["gltf"]["buffers"].is_array()) {
            for (int i = reducedState["gltf"]["buffers"].size() - 1; i >= 0; --i) {
                auto trimBufferDescription = nlohmann::json({ { "trim_buffer", nlohmann::json::object() } });
                trimBufferDescription["trim_buffer"]["path"] = "/gltf/buffers/" + std::to_string(i);

                reducedState = library.call(std::move(trimBufferDescription), std::move(reducedState), reducedDatastorage, callback);
            }
        }

        // save the subtree glTF and associated data
        auto saveTaskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
        saveTaskDescription["save_file"]["filepath"] = params["filepath"];
        saveTaskDescription["save_file"]["overwrite"] = params["overwrite"];

        library.call(std::move(saveTaskDescription), std::move(reducedState), reducedDatastorage, callback);

        // return the untouched state since we do not want to change the glTF in shown Gestaltor in any way
        return state;
    };

    return operation;
}

Operation file::exportAccessor(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "export_accessor";
    operation.schema = schema::operation(
        "Export Accessor",
        "Export accessor to readable text file",
        schema::object(
            { schema::parameter::path("(/gltf/accessors/\\d+)"), { "filepath", schema::string(".*\\.txt") } }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto accessorPath = nlohmann::json::json_pointer(params["path"]);
        std::filesystem::path filepath(params["filepath"].get<std::string>());

        auto accessor = state.at(accessorPath);
        auto componentType = accessor.at("componentType").get<uint32_t>();
        if (componentType != 5126) { // FLOAT
            throw std::invalid_argument("Unsupported accessor format");
        }
        uint32_t componentCount = 0;
        if (accessor.at("type").get<std::string>() == "SCALAR") {
            componentCount = 1;
        }
        if (accessor.at("type").get<std::string>() == "VEC2") {
            componentCount = 2;
        }
        if (accessor.at("type").get<std::string>() == "VEC3") {
            componentCount = 3;
        }
        if (accessor.at("type").get<std::string>() == "VEC4") {
            componentCount = 4;
        }

        auto count = accessor.at("count").get<uint32_t>();
        std::vector<uint8_t> accessorData = reactor::utils::getAccessorByteDataPacked(accessorPath, state, datastorage);

        if (count * componentCount * sizeof(float) != accessorData.size()) {
            throw std::invalid_argument("Accessor data mismatch");
        }

        const float* floatData = reinterpret_cast<const float*>(accessorData.data());

        std::ofstream filestream(filepath);

        filestream << "componentType " << componentType << std::endl;
        filestream << "componentCount " << componentCount << std::endl;
        filestream << "count " << count << std::endl;
        filestream << std::endl;

        for (uint32_t i = 0; i < count; i++) {
            filestream << "[ ";
            for (uint32_t c = 0; c < componentCount; c++) {
                float value = floatData[i * componentCount + c];
                filestream << value << " ";
            }
            filestream << "]" << std::endl;
        }
        filestream.close();
        return state;
    };

    return operation;
}

Operation file::importImage(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "import_image";

    // fill the operation schema header
    operation.schema = schema::operation(
        "Import Image",
        "Import an image file",
        schema::object(
            { { "filepath", schema::string(".*\\.png|.*\\.jpg|.*\\.jpeg|.*\\.webp") } }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (!state.contains("gltf")) {
            throw std::invalid_argument("No glTF given");
        }

        std::filesystem::path imagePath(params["filepath"].get<std::string>());

        if (!std::filesystem::exists(imagePath)) {
            throw std::invalid_argument("Image not found at specified file path");
        }

        if (!state.contains(nlohmann::json::json_pointer("/gltf/images"))) {
            state["gltf"]["images"] = nlohmann::json::array();
        }
        auto image = nlohmann::json::object();
        image["name"] = imagePath.stem().string();
        image["uri"] = imagePath.stem().string() + imagePath.extension().string();
        state["gltf"]["images"].emplace_back(image);

        auto data = readFile(imagePath);
        auto dataIDPath = reactor::utils::getDataIDPath(image, state);
        datastorage::DataStorage::ID dataStorageID = datastorage.insert(data);
        state[dataIDPath] = dataStorageID;
        return state;
    };

    return operation;
}

Operation file::importTexture(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "import_texture";

    // fill the operation schema header
    operation.schema = schema::operation(
        "Import Image as Texture",
        "Import an image file as texture (automatically creates texture and adds image to it)",
        schema::object(
            { { "filepath", schema::string(".*\\.png|.*\\.jpg|.*\\.jpeg") } }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (!state.contains("gltf")) {
            throw std::invalid_argument("No glTF given");
        }

        // first load image
        auto taskDescription = nlohmann::json({ { "import_image", nlohmann::json::object() } });
        taskDescription["import_image"]["filepath"] = params["filepath"].get<std::string>();
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);

        // second create texture and add image to it

        taskDescription = nlohmann::json({ { "create_gltf_component", nlohmann::json::object() } });
        taskDescription["create_gltf_component"]["path"] = "/gltf/textures";
        taskDescription["create_gltf_component"]["name"] = state["gltf"]["images"][state["gltf"]["images"].size() - 1]["name"];
        taskDescription["create_gltf_component"]["source"] = state["gltf"]["images"].size() - 1;

        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        return state;
    };

    return operation;
}

Operation file::exportImage(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "export_image";

    operation.schema = schema::operation(
        "Export Image",
        "Export image to file",
        schema::object(
            { { schema::parameter::path("/gltf/images/\\d+") }, { "filepath", schema::string(".*\\.png|.*\\.jpg") } }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());
        if (!state.contains(path)) {
            throw std::invalid_argument("Image not found");
        }
        {
            auto params = nlohmann::json::object();
            params["path"] = path.to_string();
            params["storageType"] = "File";
            auto taskDescription = nlohmann::json({ { "convert_storage_type", params } });
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        std::vector<uint8_t> rawData = reactor::utils::getImageData(path, state, datastorage);
        auto mimeType = reactor::image::getMimeType(rawData);
        std::string fileExtension = "." + reactor::utils::splitString(params["filepath"].get<std::string>(), ".").back();
        if (fileExtension != reactor::image::getFileExtension(mimeType)) {
            auto params = nlohmann::json::object();
            params["path"] = path.to_string();
            params["mimeType"] = fileExtension == ".jpg" ? "image/jpeg" : "image/png";
            auto taskDescription = nlohmann::json({ { "convert_image", params } });
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        auto dataIDPath = reactor::utils::getDataIDPath(path, state);

        uint64_t dataStorageID = state.at(dataIDPath);
        if (!datastorage.contains(dataStorageID)) {
            throw std::invalid_argument("Image data not found");
        }
        const std::vector<uint8_t>& data = datastorage.at(dataStorageID);

        std::filesystem::path filePath(params["filepath"].get<std::string>());
        file::writeFile(filePath, data, true);

        return state;
    };

    return operation;
}

Operation file::exportUsdz {
    .name = "export_usdz",

    .schema = schema::operation(
        "Export USDZ",
        "Export to USDZ",
        schema::object({
            { "filepath", schema::string(".*\\.usdz") },
            { "overwrite", schema::boolean(false) },
        })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::filesystem::path usdPath = params["filepath"].get<std::string>();

        if (!params["overwrite"].get<bool>() && std::filesystem::exists(usdPath)) {
            throw std::runtime_error("USDZ file alread exists");
        }

        // usd from gltf only works with these extensions in extensions required:
        // KHR_materials_unlit, KHR_materials_pbrSpecularGlossiness, KHR_texture_transform, KHR_draco_mesh_compression
        // check if other extensions are present

        if (state.at("gltf").contains("extensionsRequired")) {
            auto extensionsRequired = state.at("gltf").at("extensionsRequired");
            for (const auto& extension : extensionsRequired) {
                if (extension != "KHR_materials_unlit" && extension != "KHR_materials_pbrSpecularGlossiness" && extension != "KHR_texture_transform" && extension != "KHR_draco_mesh_compression") {
                    throw std::runtime_error("USDZ export only supports KHR_materials_unlit, KHR_materials_pbrSpecularGlossiness, KHR_texture_transform, KHR_draco_mesh_compression extensions in extensionsRequired.");
                }
            }
        }

        auto tmpGltfPath = std::filesystem::temp_directory_path() / "gestaltor_usd_export.glb";

        auto taskDescription = nlohmann::json({
            {
                "save_file",
                {
                    { "filepath", tmpGltfPath.string() },
                    { "overwrite", true },
                },
            },
        });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);

        // Let usd_from_gltf write into a temporary location to avoid sandboxing issues.
        auto tmpUsdPath = std::filesystem::temp_directory_path() / "gestaltor_usd_export.usdz";

        subprocess::CommandLine commandLine;

#ifdef UX3D_IS_WINDOWS
        char dllPath[MAX_PATH];
        if (GetModuleFileNameA((HINSTANCE)&__ImageBase, dllPath, MAX_PATH)) {
            std::string pathToDLL(dllPath);
            reactor::utils::searchAndReplace(pathToDLL, "Operations.dll", "usd_from_gltf.exe");
            commandLine.push_back(pathToDLL);
        }
#elif UX3D_IS_LINUX
        auto executablePath = std::filesystem::canonical("/proc/self/exe");
        executablePath = executablePath.replace_filename("usd_from_gltf");
        commandLine.push_back(executablePath.generic_string());
#elif UX3D_IS_APPLE
        char executablePathBuffer[PATH_MAX];
        uint32_t bufsize = PATH_MAX;
        if (_NSGetExecutablePath(executablePathBuffer, &bufsize) != 0) {
            throw new std::runtime_error("Could not get macOS executable path.");
        }

        auto executablePath = std::filesystem::path(executablePathBuffer);
        commandLine.push_back(executablePath.parent_path() / "usd_from_gltf");
#else
#error
#endif

        commandLine.push_back(tmpGltfPath.string());
        commandLine.push_back(tmpUsdPath.string());
        commandLine.push_back("--png_level");
        commandLine.push_back("1");
        commandLine.push_back("--image_limit_total");
        commandLine.push_back("0");

        auto completion = subprocess::RunBuilder { std::move(commandLine) }
                              .check(true)
                              .cout(subprocess::PipeOption::pipe)
                              .cerr(subprocess::PipeOption::pipe)
                              .run();

        if (completion.returncode != 0) {
            throw std::runtime_error("usd_from_gltf returned non-zero exit code");
        }

        std::filesystem::copy_file(tmpUsdPath, usdPath, std::filesystem::copy_options::overwrite_existing);

        std::filesystem::remove(tmpUsdPath);

        return state;
    }
};

Operation file::reloadFile(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "reload_file";
    auto parameters = nlohmann::json::array();

    parameters.push_back(schema::object({ schema::parameter::path("/gltf/images/\\d+"), { "filepath", schema::string() } }));
    parameters.push_back(schema::object({ schema::parameter::path("/gltf/images/\\d+"), { "resourceDirectory", schema::string() } }));

    operation.schema = schema::operation(
        "Reload File",
        "Reloads file data from resource directory or from specific file path",
        schema::oneOf(parameters));

    // define the operation function
    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());
        if (!state.contains(path / "uri")) {
            throw std::runtime_error("No uri found");
        }
        if (reactor::utils::isHashedDataUri(state.at(path / "uri").get<std::string>())) {
            throw std::runtime_error("Data is stored base64 encoded");
        }

        std::filesystem::path filepath;
        if (params.contains("resourceDirectory")) {
            std::filesystem::path uriPath(state.at(path / "uri").get<std::string>());
            std::filesystem::path resourcePath(params.at("resourceDirectory").get<std::string>());
            filepath = resourcePath / uriPath;
        } else {
            filepath = params.at("filepath").get<std::string>();
        }

        // std::cout << "reloading: " << imagePath << std::endl;

        if (!std::filesystem::exists(filepath)) {
            throw std::invalid_argument("File does not exist: " + filepath.string());
        }
        auto rawData = readFile(filepath);
        auto imagedata = reactor::image::decode(rawData);

        if (!imagedata.info.contains("width")) {
            throw std::invalid_argument("Image data invalid");
        }

        datastorage::DataStorage::ID dataStorageID = datastorage.insert(rawData);
        auto dataIDPath = reactor::utils::getDataIDPath(path, state);
        state[dataIDPath] = dataStorageID;
        return state;
    };

    return operation;
}
