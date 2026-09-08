#pragma once
#include <list>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <stack>
#include <string>
#include <unordered_map>

namespace ux3d::editor {

// https://doc.qt.io/qt-5/qtwidgets-itemviews-simpletreemodel-example.html
class JsonTreeElement {
public:
    JsonTreeElement();
    JsonTreeElement(const std::string& path, const nlohmann::json& key, const nlohmann::json& value, JsonTreeElement* parent = nullptr);
    JsonTreeElement(const JsonTreeElement& source) = delete;
    JsonTreeElement& operator=(const JsonTreeElement& other) = delete;
    JsonTreeElement(JsonTreeElement&& move) noexcept;
    JsonTreeElement& operator=(JsonTreeElement&& move) noexcept;

    ~JsonTreeElement();

    JsonTreeElement& addProperty(const std::string& path, const nlohmann::json& key, const nlohmann::json& value, float sorting = 0.0f);
    void sortProperties();
    JsonTreeElement* getProperty(const std::string& path);
    JsonTreeElement* getProperty(int index);
    const JsonTreeElement* getProperty(const std::string& path) const;
    const JsonTreeElement* getProperty(int index) const;

    JsonTreeElement* getPropertyRecursive(const nlohmann::json::json_pointer& path);

    void removeProperty(JsonTreeElement* property);
    void removeProperty(std::list<std::tuple<float, JsonTreeElement>>::iterator iter);

    std::optional<std::list<std::tuple<float, JsonTreeElement>>::iterator> getIterator(const std::string& path);
    std::optional<std::list<std::tuple<float, JsonTreeElement>>::const_iterator> getIterator(const std::string& path) const;
    int getIndex(const std::string& path);
    size_t getPropertyCount() const;

    nlohmann::json key;
    nlohmann::json value;
    std::string path = "";
    JsonTreeElement* parent = nullptr;

    bool operator>(const JsonTreeElement& element) const
    {
        return path > element.path;
    }
    bool operator<(const JsonTreeElement& element) const
    {
        return path < element.path;
    }

    auto begin()
    {
        return properties.begin();
    }

    auto cbegin() const
    {
        return properties.cbegin();
    }

    auto end()
    {
        return properties.end();
    }

    auto cend() const
    {
        return properties.cend();
    }

private:
    // TODO: this has been converted from std::vector to std::list to make sure JsonTreeElement pointers don't
    // point to garbage memory. This has a severe drawback on performance in cases where there are many properties (e.g. a node hierarchy)
    // that e.g. are searched or iterated through
    std::list<std::tuple<float, JsonTreeElement>> properties;
    std::unordered_map<std::string, std::tuple<size_t, std::list<std::tuple<float, JsonTreeElement>>::iterator>> indexMapping;
    mutable std::mutex mtx;

    JsonTreeElement* getPropertyRecursiveHelper(std::stack<std::string>& remainingPointerElements);
};
}
