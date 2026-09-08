#include "reactor/Utils.h"
#include "datastorage/DataStorage.h"
#include "reactor/GltfEnums.h"
#include "reactor/ImageOperations.h"

#include <optional>
#include <uuid.h>

using namespace ux3d;
using namespace ux3d::reactor;

std::vector<std::string> utils::splitString(std::string s, const std::string& delimiter)
{
    std::vector<std::string> output;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        output.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    output.push_back(s);
    return output;
}

std::string utils::mergeString(const std::vector<std::string>& tokens, const std::string& separator)
{
    std::string combined = "";
    for (int i = 0; i < tokens.size() - 1; i++) {
        combined = combined + tokens[i] + separator;
    }
    combined = combined + tokens[tokens.size() - 1];
    return combined;
}

void utils::searchAndReplace(std::string& str, const std::string& search, const std::string& replace)
{
    if (search.empty()) {
        return;
    }
    size_t currentPosition = 0;
    while ((currentPosition = str.find(search, currentPosition)) != std::string::npos) {
        str.replace(currentPosition, search.length(), replace);
        currentPosition += replace.length();
    }
}

bool utils::hasElement(const nlohmann::json& json, std::vector<std::variant<std::string, int>> path)
{
    nlohmann::json subtree = json;
    for (int i = 0; i < path.size(); i++) {
        if (subtree.is_array() && std::holds_alternative<int>(path[i])) {
            int index = std::get<int>(path[i]);
            if (subtree.size() <= index) {
                return false;
            }
            subtree = subtree.at(index);
        } else if (subtree.is_object() && std::holds_alternative<std::string>(path[i])) {
            std::string key = std::get<std::string>(path[i]);
            if (!subtree.contains(key)) {
                return false;
            }
            subtree = subtree.at(key);
        } else {
            return false;
        }
    }
    return true;
}

void utils::createSubElement(nlohmann::json& json, std::vector<std::variant<std::string, int>> path)
{
    nlohmann::json::json_pointer subtreePath;
    for (int i = 0; i < path.size(); i++) {
        if (std::holds_alternative<int>(path[i])) {
            int index = std::get<int>(path[i]);
            if (!json[subtreePath].is_array()) {
                json[subtreePath] = nlohmann::json::array();
            }
            if (index < 0) {
                return;
            }
            if (json[subtreePath].size() <= index) {
                json[subtreePath].get_ptr<nlohmann::json::array_t*>()->resize(index);
            }
            subtreePath = subtreePath / index;
        } else if (std::holds_alternative<std::string>(path[i])) {
            std::string key = std::get<std::string>(path[i]);
            if (!json[subtreePath].is_object()) {
                json[subtreePath] = nlohmann::json::object();
            }
            if (key.empty()) {
                return;
            }
            if (!json[subtreePath].contains(key)) {
                json[subtreePath][key] = nlohmann::json::value_t::null;
            }
            subtreePath = subtreePath / key;
        } else {
            return;
        }
    }
}

bool is_number(const std::string& s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

void utils::createSubElement(nlohmann::json& json, std::string path)
{
    auto tokens = utils::splitString(path, "/");
    std::vector<std::variant<std::string, int>> variant_tokens;
    for (auto& token : tokens) {
        if (token.empty()) {
            continue;
        }
        if (is_number(token)) {
            int i = std::stoi(token);
            variant_tokens.push_back(i);
        } else {
            variant_tokens.push_back(token);
        }
    }
    utils::createSubElement(json, variant_tokens);
}

bool utils::isHashedDataUri(std::string uri)
{
    if (uri.empty()) {
        return false;
    }
    std::transform(uri.begin(), uri.end(), uri.begin(), [](unsigned char c) { return std::tolower(c); });

    if (uri.rfind("base64_embedded_data_", 0) == 0) {
        return true;
    }

    return false;
}

bool utils::isDataUri(std::string uri)
{
    if (uri.empty()) {
        return false;
    }
    std::transform(uri.begin(), uri.end(), uri.begin(), [](unsigned char c) { return std::tolower(c); });

    if (uri.rfind("data:", 0) == 0) {
        return true;
    }

    return false;
}

nlohmann::json::json_pointer utils::getDataIDPath(const nlohmann::json::json_pointer propertyPointer, const nlohmann::json& state)
{
    return getDataIDPath(state.at(propertyPointer), state);
}

std::tuple<std::string, std::string> utils::getUUIDLocalPath(nlohmann::json::json_pointer pointer, const nlohmann::json& stateWithUUIDs)
{
    std::tuple<std::string, std::string> result;
    if (!stateWithUUIDs.contains(pointer)) {
        throw(std::invalid_argument("Value does not exists in state"));
    }
    const nlohmann::json* currentProperty = &stateWithUUIDs.at(pointer);
    std::string& localPath = std::get<1>(result);
    while (!currentProperty->contains("uuid")) {
        localPath = "/" + pointer.back() + localPath;
        pointer.pop_back();
        currentProperty = &stateWithUUIDs.at(pointer);
    }
    std::get<0>(result) = currentProperty->at("uuid");
    return result;
}

// The returned nlohmann::json::json_pointer dataIDPath is a path inside the state pointing to the location where the dataStorageID is stored
nlohmann::json::json_pointer utils::getDataIDPath(const nlohmann::json& _property, const nlohmann::json& state)
{
    const nlohmann::json* property = &_property;
    if (property->contains("bufferView")) {
        int bufferView = property->at("bufferView");
        int buffer = state.at("/gltf/bufferViews"_json_pointer / bufferView / "buffer");
        property = &state.at("/gltf/buffers"_json_pointer / buffer);
    }
    std::string dataCategory;
    std::string dataID;
    if (property->contains("uri") && !property->at("uri").empty()) {
        std::string uri = property->at("uri").get<std::string>();
        // On load/convert, embedded strings are replaced by a specific marker (base64_embedded_data_) + data hash
        // With this uri, one can lookup the decoded data in the DataStorage
        // On save, this marker URI gets replaced with an embedded string, again.
        if (uri.find("base64_embedded_data_") != std::string::npos) {
            dataCategory = "embedded";
        } else {
            dataCategory = "files";
        }
        dataID = uri;
    } else {
        dataCategory = "glb";
        dataID = "glb_buffer";
    }
    return nlohmann::json::json_pointer("") / dataCategory / dataID;
}

void utils::updateBufferData(nlohmann::json::json_pointer bufferPath, std::vector<uint8_t>& data, nlohmann::json& state, datastorage::DataStorage& datastorage)
{
    if (!state.contains(bufferPath)) {
        return;
    }
    nlohmann::json::json_pointer dataIDPath = utils::getDataIDPath(bufferPath, state);

    nlohmann::json& buffer = state.at(bufferPath);
    buffer["byteLength"] = data.size();

    // Add new data to datastorage and update datastorageID
    datastorage::DataStorage::ID datastorageID = datastorage.insert(std::move(data));
    state[dataIDPath] = datastorageID;
}

std::vector<uint8_t> utils::getBufferViewData(nlohmann::json::json_pointer bufferViewPath, const nlohmann::json& state, const datastorage::DataStorage& datastorage)
{
    auto& bufferView = state[bufferViewPath];
    nlohmann::json::json_pointer bufferPath("/gltf/buffers");
    bufferPath = bufferPath / bufferView["buffer"].get<uint32_t>();
    nlohmann::json::json_pointer dataIDPath = utils::getDataIDPath(bufferPath, state);
    uint64_t dataStorageID = state.at(dataIDPath);
    const std::vector<uint8_t>& bufferData = datastorage.at(dataStorageID);

    uint32_t byteLength = bufferView["byteLength"];
    uint32_t byteOffset = 0u;
    if (bufferView.contains("byteOffset")) {
        byteOffset = bufferView["byteOffset"];
    }
    if (bufferData.size() < byteOffset + byteLength) {
        throw std::runtime_error("Buffer data invalid");
    }
    std::vector<uint8_t> data(byteLength);
    data.assign(bufferData.begin() + byteOffset, bufferData.begin() + byteOffset + byteLength);
    return data;
}

std::vector<uint8_t> utils::getAccessorByteDataPacked(nlohmann::json::json_pointer accessorPath, const nlohmann::json& state, const datastorage::DataStorage& datastorage)
{
    auto& accessor = state[accessorPath];

    uint32_t count = accessor["count"];
    uint32_t elementSize = gltf::elementSize(accessor);
    uint32_t packedSize = count * elementSize;

    std::vector<uint8_t> data;
    if (!accessor.contains("bufferView")) {
        data.resize(packedSize, 0); // Spec: When undefined, the accessor MUST be initialized with zeros;
        return data;
    }

    uint32_t idx = accessor.at("bufferView").get<uint32_t>();
    nlohmann::json::json_pointer bufferViewPath("/gltf/bufferViews");
    auto& bufferView = state[bufferViewPath / idx];
    std::vector<uint8_t> bufferViewData = utils::getBufferViewData(bufferViewPath / idx, state, datastorage);
    if (bufferViewData.size() < packedSize) {
        throw std::runtime_error("Invalid accessor");
    }

    // If no bufferView stride is given, data is tightly packed (no padding)
    uint32_t bufferViewStride = gltf::elementSize(accessor);
    if (bufferView.contains("byteStride")) {
        bufferViewStride = bufferView.at("byteStride").get<uint32_t>();
    }

    uint32_t byteOffset = 0;
    if (accessor.contains("byteOffset")) {
        byteOffset = accessor.at("byteOffset").get<uint32_t>();
    }

    uint32_t currentOffset = byteOffset;
    for (uint32_t i = 0; i < count; i++) {
        data.insert(std::end(data), std::begin(bufferViewData) + currentOffset, std::begin(bufferViewData) + currentOffset + elementSize);
        currentOffset += bufferViewStride;
    }

    return data;
}

std::vector<uint8_t> utils::getImageData(nlohmann::json::json_pointer imagePath, const nlohmann::json& state, const datastorage::DataStorage& datastorage)
{
    if (!state.contains(imagePath)) {
        throw std::invalid_argument("Image not found");
    }
    const auto& image = state[imagePath];
    if (image.contains("uri")) {
        return datastorage.at(state.at(utils::getDataIDPath(imagePath, state)));
    } else if (image.contains("bufferView")) {
        uint32_t index = image.at("bufferView").get<uint32_t>();
        auto pointer = nlohmann::json::json_pointer("/gltf/bufferViews") / index;
        if (!state.contains(pointer)) {
            throw std::invalid_argument("Image buffer view not found");
        }
        return getBufferViewData(pointer, state, datastorage);
    } else {
        throw std::invalid_argument("Image has neither URI nor buffer view");
    }
}

image::ImageData utils::decodeImageData(nlohmann::json::json_pointer imagePath, const nlohmann::json& state, const datastorage::DataStorage& datastorage)
{
    return image::decode(utils::getImageData(imagePath, state, datastorage));
}

void utils::setImageData(nlohmann::json::json_pointer imagePath, const image::ImageData& imageData, nlohmann::json& state, datastorage::DataStorage& datastorage)
{
    if (!state.contains(imagePath)) {
        throw std::invalid_argument("Image not found");
    }
    auto& image = state[imagePath];
    auto mimeType = reactor::image::getMimeType(getImageData(imagePath, state, datastorage));
    auto bytes = reactor::image::encode(imageData, { { "mimeType", mimeType } });
    state[reactor::utils::getDataIDPath(image, state)] = datastorage.insert(std::move(bytes));
}

// create new buffer with given data and add it to state and datastorage
// return path of newly created buffer
nlohmann::json::json_pointer utils::createBuffer(nlohmann::json& state, datastorage::DataStorage& datastorage, const std::vector<uint8_t>& data, bool createEmbedded, std::string uri)
{
    // Make sure that buffers are available
    reactor::utils::createSubElement(state, { "gltf", "buffers", -1 });

    // Create new buffer:
    nlohmann::json newBuffer = {};
    newBuffer["byteLength"] = data.size();

    if (createEmbedded) {
        newBuffer["uri"] = std::string("base64_embedded_data_").append(reactor::utils::getRandomString());
    } else {
        if (uri.empty() || utils::isURIUsed(uri, state)) {
            newBuffer["uri"] = reactor::utils::createURIStem(state) + ".bin";
        } else {
            newBuffer["uri"] = uri;
        }
    }
    uint32_t newID = state.at(nlohmann::json::json_pointer("/gltf/buffers")).size();

    state.at(nlohmann::json::json_pointer("/gltf/buffers")).push_back(newBuffer); // add newly created buffer
    nlohmann::json::json_pointer bufferPath("/gltf/buffers");
    bufferPath = bufferPath / newID;

    // Add newly created data to datastorage
    nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(bufferPath, state);
    datastorage::DataStorage::ID dataID = datastorage.insert(data);
    state[dataIDPath] = dataID;

    return bufferPath;
}

// create new bufferView that references the given buffer
nlohmann::json::json_pointer utils::createBufferView(nlohmann::json& state, nlohmann::json::json_pointer bufferPath)
{
    // Make sure that bufferViews are available
    reactor::utils::createSubElement(state, { "gltf", "bufferViews", -1 });

    // Create new buffer:
    nlohmann::json bufferView = {};
    bufferView["byteLength"] = state.at(bufferPath)["byteLength"];
    bufferView["buffer"] = stoi(bufferPath.back());

    auto bufferViewPath = nlohmann::json::json_pointer("/gltf/bufferViews");
    state.at(bufferViewPath).push_back(bufferView);

    return bufferViewPath / (state.at(bufferViewPath).size() - 1);
}

// create new bufferView that references the provided data and appends the data to the buffer
nlohmann::json::json_pointer utils::appendBufferView(nlohmann::json::json_pointer bufferPath, const std::vector<uint8_t>& data, nlohmann::json& state, datastorage::DataStorage& datastorage)
{
    if (!state.contains(bufferPath)) {
        throw std::runtime_error("Buffer path invalid");
    }

    nlohmann::json::json_pointer dataIDPath = utils::getDataIDPath(bufferPath, state);
    uint64_t dataStorageID = state.at(dataIDPath);
    const std::vector<uint8_t>& bufferData = datastorage.at(dataStorageID);

    uint64_t byteOffset = bufferData.size();

    std::vector<uint8_t> mergedData = bufferData;
    mergedData.insert(std::end(mergedData), std::begin(data), std::end(data));

    utils::updateBufferData(bufferPath, mergedData, state, datastorage);

    reactor::utils::createSubElement(state, { "gltf", "bufferViews", -1 });
    nlohmann::json bufferView = {};
    bufferView["byteLength"] = data.size();
    bufferView["byteOffset"] = byteOffset;
    bufferView["buffer"] = stoi(bufferPath.back());

    auto bufferViewPath = nlohmann::json::json_pointer("/gltf/bufferViews");
    state.at(bufferViewPath).push_back(bufferView);

    return bufferViewPath / (state.at(bufferViewPath).size() - 1);
}

size_t utils::getAccessorByteSize(nlohmann::json::json_pointer accessorPath, const nlohmann::json& state)
{
    auto& accessor = state.at(accessorPath);
    return ux3d::reactor::gltf::elementSize(accessor) * accessor["count"].get<uint32_t>();
}

utils::AccessorTypeVariant
utils::getAccessorData(nlohmann::json::json_pointer accessorPath, const nlohmann::json& state, const datastorage::DataStorage& datastorage)
{
    if (!state.contains(accessorPath)) {
        return std::monostate();
    }

    const auto& accessor = state.at(accessorPath);
    const std::string attributeType = accessor.at("type").get<std::string>();
    const gltf::ComponentType componentType = accessor.at("componentType").get<gltf::ComponentType>();

    if (attributeType == "VEC2" && componentType == gltf::ComponentType::Float) {
        return getAccessorData<glm::vec2>(accessorPath, state, datastorage);
    }
    if (attributeType == "VEC3" && componentType == gltf::ComponentType::Float) {
        return getAccessorData<glm::vec3>(accessorPath, state, datastorage);
    }
    if (attributeType == "VEC4" && componentType == gltf::ComponentType::Float) {
        return getAccessorData<glm::vec4>(accessorPath, state, datastorage);
    }
    if (attributeType == "SCALAR" && componentType == gltf::ComponentType::Float) {
        return getAccessorData<float>(accessorPath, state, datastorage);
    }
    if (attributeType == "SCALAR" && componentType == gltf::ComponentType::UnsignedInt) {
        return getAccessorData<uint32_t>(accessorPath, state, datastorage);
    }
    if (attributeType == "SCALAR" && componentType == gltf::ComponentType::UnsignedShort) {
        return getAccessorData<uint16_t>(accessorPath, state, datastorage);
    }
    if (attributeType == "SCALAR" && componentType == gltf::ComponentType::UnsignedByte) {
        return getAccessorData<uint8_t>(accessorPath, state, datastorage);
    }

    return std::monostate();
}

nlohmann::json::json_pointer utils::createAccessorFromData(
    std::string attributeType,
    gltf::ComponentType componentType,
    uint32_t count,
    const std::vector<uint8_t>& data,
    nlohmann::json& state, datastorage::DataStorage& datastorage)
{
    uint32_t bufferViewIndex;
    if (state.contains(nlohmann::json::json_pointer("/gltf/buffers/0"))) {
        // if glTF already contains a buffer, we do not create a new buffer
        auto bufferViewPath = appendBufferView(nlohmann::json::json_pointer("/gltf/buffers/0"), data, state, datastorage);
        bufferViewIndex = std::stoi(bufferViewPath.back());
    } else {
        auto bufferPath = createBuffer(state, datastorage, data);
        auto bufferViewPath = createBufferView(state, bufferPath);
        bufferViewIndex = std::stoi(bufferViewPath.back());
    }

    // Make sure that accessors array is available
    reactor::utils::createSubElement(state, { "gltf", "accessors", -1 });

    nlohmann::json accessor = {};
    accessor["count"] = count;
    accessor["bufferView"] = bufferViewIndex;
    accessor["componentType"] = componentType;
    accessor["type"] = attributeType;

    auto accessorPath = nlohmann::json::json_pointer("/gltf/accessors");
    state.at(accessorPath).push_back(accessor);

    return accessorPath / (state.at(accessorPath).size() - 1);
}

std::string utils::getRandomString()
{
    // Initialize UUID random number generator and return random string
    std::random_device rd;
    auto seed_data = std::array<int, std::mt19937::state_size> {};
    std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
    std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
    std::mt19937 generator(seq);
    uuids::uuid_random_generator generateUUID { generator };
    return uuids::to_string(generateUUID());
}

bool utils::isURIUsed(std::string uri, const nlohmann::json& state)
{
    if (state.contains("files")) {
        auto& files = state.at("files");
        for (const auto& [key, value] : files.items()) {
            if (uri.compare(key) == 0) {
                return true;
            }
        }
    }
    return false;
}

bool utils::isURIStemUsed(std::string uriStem, const nlohmann::json& state)
{
    if (state.contains("files")) {
        auto& files = state.at("files");
        for (const auto& [key, value] : files.items()) {
            if (uriStem.compare(splitString(key, ".")[0]) == 0) {
                // compare without file extension as this function checks availability regardless of file type
                return true;
            }
        }
    }
    return false;
}

std::string utils::createURIStem(const nlohmann::json& state)
{
    const std::string uriPrefix = "file_";
    const uint32_t maxIndex = 1000;

    for (uint32_t i = 0; i < maxIndex; i++) {
        std::string candidate = uriPrefix + std::to_string(i);
        if (!utils::isURIStemUsed(candidate, state)) {
            return candidate;
        }
    }
    // If we were unable to find an unused uri within given indices a random string is used as fallback
    return getRandomString();
}

std::vector<nlohmann::json::json_pointer> utils::getReferencedAccessors(const nlohmann::json& primitive)
{
    // info: this function does not return referenced morph target accessors
    std::vector<nlohmann::json::json_pointer> pathVector;
    nlohmann::json::json_pointer accessorPath("/gltf/accessors");

    if (primitive.contains("indices")) {
        uint32_t idx = primitive.at("indices").get<uint32_t>();
        pathVector.push_back(accessorPath / idx);
    }

    if (primitive.contains("attributes")) {
        auto& attributes = primitive.at("attributes");
        for (auto& [key, attribute] : attributes.items()) {
            uint32_t idx = attribute.get<uint32_t>();
            pathVector.push_back(accessorPath / idx);
        }
    }
    return pathVector;
}

void utils::replaceAccessorInPrimitive(const int oldAccessorIDX, const int newAccessorIDX, nlohmann::json& state, const nlohmann::json::json_pointer& primitivePath)
{
    if (state.at(primitivePath).contains("indices")) {
        uint32_t idx = state.at(primitivePath).at("indices").get<uint32_t>();
        if (idx == oldAccessorIDX) {
            state.at(primitivePath)["indices"] = newAccessorIDX;
        }
    }

    if (state.at(primitivePath).contains("attributes")) {
        auto& attributes = state.at(primitivePath).at("attributes");
        for (auto& [key, attribute] : attributes.items()) {
            uint32_t idx = attribute.get<uint32_t>();

            if (idx == oldAccessorIDX) {
                attributes[key] = newAccessorIDX;
            }
        }
    }
}

std::vector<std::tuple<nlohmann::json::json_pointer, std::string>> utils::getMeshQuantizationAccessors(const nlohmann::json& state)
{
    std::vector<std::tuple<nlohmann::json::json_pointer, std::string>> result;
    if (!state.contains("/gltf/meshes"_json_pointer)) {
        return result;
    }

    auto getPathAndAttribute = [&state](const std::string& attributeKey, int value, const auto& typeMap) -> std::optional<std::tuple<nlohmann::json::json_pointer, std::string>> {
        const std::string parsedAttributeKey = attributeKey.substr(0, 8); // handle TEXCOORD_n by cutting it down to "TEXCOORD"
        if (!typeMap.contains(parsedAttributeKey)) {
            return std::nullopt;
        }

        // get mesh quantization types for this attribute
        auto typeVector = typeMap.at(parsedAttributeKey);

        nlohmann::json::json_pointer accessorPath("/gltf/accessors/" + std::to_string(value));
        if (!state.contains(accessorPath)) {
            return std::nullopt;
        }

        const auto& accessor = state.at(accessorPath);
        if (!accessor.contains("componentType")) {
            return std::nullopt;
        }

        gltf::ComponentType componentType = static_cast<gltf::ComponentType>(accessor.at("componentType").get<int>());
        bool isNormalized = false;
        if (accessor.contains("normalized")) {
            isNormalized = accessor["normalized"].get<bool>();
        }
        auto accessorData = std::make_tuple(componentType, isNormalized);
        for (const auto& meshQuantData : typeVector) {
            if (accessorData == meshQuantData) {
                return std::make_tuple(accessorPath, parsedAttributeKey);
            }
        }

        return std::nullopt;
    };

    for (const auto& mesh : state["/gltf/meshes"_json_pointer]) {
        if (!mesh.contains("primitives")) {
            continue;
        }

        for (const auto& primitive : mesh["primitives"]) {
            if (primitive.contains("attributes")) {
                for (const auto& [attributeKey, value] : primitive["attributes"].items()) {
                    auto pathAndAttr = getPathAndAttribute(attributeKey, value.get<int>(), utils::meshQuantizationTypes);
                    if (pathAndAttr.has_value()) {
                        result.push_back(pathAndAttr.value());
                    }
                }
            }
            if (primitive.contains("targets")) {
                for (const auto& target : primitive["targets"]) {
                    for (const auto& [attributeKey, value] : target.items()) {
                        auto pathAndAttr = getPathAndAttribute(attributeKey, value.get<int>(), utils::meshQuantizationTypesMorph);
                        if (pathAndAttr.has_value()) {
                            result.push_back(pathAndAttr.value());
                        }
                    }
                }
            }
        }
    }

    return result;
}

nlohmann::json utils::extractImagePreviewFromData(const nlohmann::json::json_pointer& pointer, const std::shared_ptr<const nlohmann::json>& headPtr, const datastorage::DataStorage& datastorage)
{
    const auto& gltf = headPtr->at("gltf");

    if (gltf.contains(pointer) == false) {
        return nlohmann::json();
    }

    const std::string pointerStr = pointer.to_string();
    const nlohmann::json::json_pointer parentPointer = pointer.parent_pointer();
    const std::string parentPointerStr = parentPointer.to_string();

    if (parentPointerStr.starts_with("/textures") == false && parentPointerStr.starts_with("/images") == false) {
        return nlohmann::json();
    }

    nlohmann::json::json_pointer imagePointer = pointer;
    if (pointerStr.starts_with("/textures")) {
        const nlohmann::json& texture = gltf.at(pointer);
        if (texture.contains("source") == false) {
            return nlohmann::json();
        }
        imagePointer = nlohmann::json::json_pointer(texture["source"]);
    }

    nlohmann::json imageData = nlohmann::json();
    const auto& image = gltf.at(imagePointer);
    if (image.contains("uri")) {
        std::string uri = image.at("uri");
        if (headPtr->at("files").contains(uri)) {
            auto id = headPtr->at("files").at(uri).get<int>();
            imageData["source"] = std::to_string(id) + uri;
            imageData["size"] = datastorage.at(id).size();
        }
        if (headPtr->at("embedded").contains(uri)) {
            auto id = headPtr->at("embedded").at(uri).get<int>();
            imageData["source"] = std::to_string(id) + uri;
            imageData["size"] = datastorage.at(id).size();
        }
    } else if (image.contains("bufferView")) {
        auto bufferViewPointer = nlohmann::json::json_pointer {} / "gltf" / nlohmann::json::json_pointer { image.at("bufferView").get<std::string>() };
        imageData["size"] = (*headPtr)[bufferViewPointer]["byteLength"];
        auto bufferPointer = nlohmann::json::json_pointer {} / "gltf" / nlohmann::json::json_pointer { headPtr->value<std::string>(bufferViewPointer / "buffer", "-1") };
        if (bufferPointer.back() == "0" && !headPtr->contains(bufferPointer / "uri")) {
            auto id = headPtr->at("/glb/glb_buffer"_json_pointer).get<int>();
            imageData["source"] = std::to_string(id) + bufferViewPointer.to_string();
        } else if (bufferPointer.back() != "-1" && headPtr->contains(bufferPointer / "uri")) {
            std::string uri = headPtr->at(bufferPointer / "uri");
            auto id = headPtr->at("files").at(uri).get<int>();
            imageData["source"] = std::to_string(id) + bufferViewPointer.to_string();
        }
    }

    return imageData;
}