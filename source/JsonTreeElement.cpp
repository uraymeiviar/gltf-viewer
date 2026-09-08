#include "JsonTreeElement.h"

using namespace ux3d;
using namespace ux3d::editor;

JsonTreeElement::JsonTreeElement()
    : mtx()
{
}

JsonTreeElement::JsonTreeElement(const std::string& path, const nlohmann::json& key, const nlohmann::json& value, JsonTreeElement* parent)
    : path(path)
    , key(key)
    , value(value)
    , parent(parent)
    , mtx()
{
}

JsonTreeElement::JsonTreeElement(JsonTreeElement&& source) noexcept
{
    const std::lock_guard<std::mutex> lock(source.mtx);
    path = std::move(source.path);
    key = std::move(source.key);
    value = std::move(source.value);
    parent = std::move(source.parent);
    properties = std::move(properties);
    indexMapping = std::move(indexMapping);
}

JsonTreeElement& JsonTreeElement::operator=(JsonTreeElement&& source) noexcept
{
    const std::lock_guard<std::mutex> lock(source.mtx);
    path = std::move(source.path);
    key = std::move(source.key);
    value = std::move(source.value);
    parent = std::move(source.parent);
    properties = std::move(source.properties);
    indexMapping = std::move(source.indexMapping);
    return *this;
}

JsonTreeElement::~JsonTreeElement()
{
    mtx.lock();
    mtx.unlock();
}

JsonTreeElement& JsonTreeElement::addProperty(const std::string& path, const nlohmann::json& key, const nlohmann::json& value, float sorting)
{
    auto& inserted = properties.emplace_back(sorting, std::move(JsonTreeElement(path, key, value, this)));
    return std::get<1>(inserted);
}

void JsonTreeElement::removeProperty(JsonTreeElement* property)
{
    if (property != nullptr) {
        std::string path = property->path;
        const std::lock_guard<std::mutex> lock(mtx);
        auto it = indexMapping.find(path);
        if (it != indexMapping.end()) {
            auto propertyIter = std::get<1>(it->second);
            for (++propertyIter; propertyIter != properties.end(); ++propertyIter) {
                std::get<0>(indexMapping.at(std::get<1>(*propertyIter).path))--;
            }
            properties.erase(std::get<1>(it->second));
            indexMapping.erase(path);
        }
    }
}
void JsonTreeElement::removeProperty(std::list<std::tuple<float, JsonTreeElement>>::iterator iter)
{
    if (iter != properties.end()) {
        std::string path = std::get<1>(*iter).path;
        auto propertyIter = iter;
        const std::lock_guard<std::mutex> lock(mtx);
        for (++propertyIter; propertyIter != properties.end(); ++propertyIter) {
            std::get<0>(indexMapping.at(std::get<1>(*propertyIter).path))--;
        }
        properties.erase(iter);
        indexMapping.erase(path);
    }
}

void JsonTreeElement::sortProperties()
{
    const std::lock_guard<std::mutex> lock(mtx);
    properties.sort();
    properties.reverse();
    indexMapping.clear();
    for (int i = 0; i < properties.size(); ++i) {
        auto it = properties.begin();
        std::advance(it, i);
        indexMapping.emplace(std::get<1>(*it).path, std::make_tuple(i, it));
        std::get<1>(*it).sortProperties();
    }
}

int JsonTreeElement::getIndex(const std::string& path)
{
    const std::lock_guard<std::mutex> lock(mtx);
    if (indexMapping.contains(path)) {
        return std::get<0>(indexMapping.at(path));
    }
    return -1;
}

std::optional<std::list<std::tuple<float, JsonTreeElement>>::iterator> JsonTreeElement::getIterator(const std::string& path)
{
    const std::lock_guard<std::mutex> lock(mtx);
    if (indexMapping.contains(path)) {
        return std::get<1>(indexMapping.at(path));
    }
    return std::nullopt;
}

std::optional<std::list<std::tuple<float, JsonTreeElement>>::const_iterator> JsonTreeElement::getIterator(const std::string& path) const
{
    const std::lock_guard<std::mutex> lock(mtx);
    if (indexMapping.contains(path)) {
        return std::get<1>(indexMapping.at(path));
    }
    return std::nullopt;
}

JsonTreeElement* JsonTreeElement::getProperty(int index)
{
    if (index >= 0 && index < properties.size()) {
        auto it = properties.begin();
        std::advance(it, index);
        return &std::get<1>(*it);
    }
    return nullptr;
}

JsonTreeElement* JsonTreeElement::getProperty(const std::string& path)
{
    auto iter = getIterator(path);
    if (!iter.has_value()) {
        return nullptr;
    }
    return &std::get<1>(*iter.value());
}

const JsonTreeElement* JsonTreeElement::getProperty(int index) const
{
    if (index >= 0 && index < properties.size()) {
        auto it = properties.begin();
        std::advance(it, index);
        return &std::get<1>(*it);
    }
    return nullptr;
}

const JsonTreeElement* JsonTreeElement::getProperty(const std::string& path) const
{
    auto iter = getIterator(path);
    if (!iter.has_value()) {
        return nullptr;
    }
    return &std::get<1>(*iter.value());
}

size_t JsonTreeElement::getPropertyCount() const
{
    return properties.size();
}

JsonTreeElement* JsonTreeElement::getPropertyRecursiveHelper(std::stack<std::string>& remainingPointerElements)
{
    if (remainingPointerElements.empty()) {
        return this;
    }
    auto element = getProperty(path + "/" + remainingPointerElements.top());
    if (element == nullptr) {
        return element;
    } else {
        remainingPointerElements.pop();
        return element->getPropertyRecursiveHelper(remainingPointerElements);
    }
}

JsonTreeElement* JsonTreeElement::getPropertyRecursive(const nlohmann::json::json_pointer& path)
{
    if (path.to_string().rfind(this->path, 0) == 0) {
        nlohmann::json::json_pointer remainingPointer(path.to_string().substr(this->path.size()));
        std::stack<std::string> remainingPointerElements;
        while (!remainingPointer.empty()) {
            remainingPointerElements.push(remainingPointer.back());
            remainingPointer.pop_back();
        }
        return getPropertyRecursiveHelper(remainingPointerElements);
    }
    return nullptr;
}