#include "reactor/Json.h"
#include <algorithm>
#include <deque>
#include <queue>
#include <regex>
#include <stack>
#include <string>
#include <typeinfo>

using namespace ux3d;
using namespace ux3d::reactor;
using namespace std::string_literals;

void json::for_each_match(const std::string& pointerRegex, const nlohmann::json& json, std::function<void(const nlohmann::json::json_pointer& pointer)> func)
{
    auto split = [](const std::string& s, const char delim) -> std::deque<std::string_view> {
        std::deque<std::string_view> result;
        int begin = 0;
        int end;
        for (end = 0; end < s.size(); ++end) {
            if (s[end] != delim) {
                continue;
            }
            result.push_back(std::string_view(&s[begin], end - begin));
            begin = end + 1;
        }
        result.push_back(std::string_view(&s[begin], end - begin));
        return result;
    };

    // split pattern path into individual tokens with which we can traverse the json.
    auto subPatterns = split(pointerRegex, '/');
    // match pattern regex to the paths in the json
    std::function<void(std::deque<std::string_view> subPatterns, const nlohmann::json::json_pointer& pointer)> traverse;
    traverse = [&, pointerRegex = pointerRegex](std::deque<std::string_view> subPatterns, const nlohmann::json::json_pointer& pointer) {
        if (!json.contains(pointer)) {
            return;
        }
        // reached the end of the pattern => store mapping
        if (subPatterns.empty()) {
            func(pointer);
            return;
        }

        auto token = subPatterns.front();
        subPatterns.pop_front();
        // skip empty tokens
        if (token.empty()) {
            traverse(subPatterns, pointer);
            return;
        }
        // iterate array tokens
        if (token == "\\d+") {
            if (!json.at(pointer).is_array()) {
                return;
            }
            // save count before because json can change during traversal
            size_t elementCount = json.at(pointer).size();
            for (int i = 0; i < elementCount; ++i) {
                traverse(subPatterns, pointer / i);
            }
            return;
        }
        if (token == "[^\\x2f]*") {
            if (json.at(pointer).is_array() || json.at(pointer).is_object()) {
                // save keys since they could change during traversal
                std::vector<std::string> keys;
                for (auto& [key, value] : json.at(pointer).items()) {
                    keys.push_back(key);
                }
                for (auto& key : keys) {
                    traverse(subPatterns, pointer / key);
                }
            }
            return;
        }
        // traverse into name tokens
        traverse(subPatterns, pointer / std::string(token));
    };
    traverse(subPatterns, nlohmann::json::json_pointer(""));
}

nlohmann::json json::keys(const nlohmann::json& json)
{
    auto keys = nlohmann::json::array();
    for (auto& [key, value] : json.items()) {
        keys.push_back(key);
    }
    return keys;
}

nlohmann::json json::values(const nlohmann::json& json)
{
    auto values = nlohmann::json::array();
    for (auto& [key, value] : json.items()) {
        values.push_back(value);
    }
    return values;
}

nlohmann::json json::unzip(const nlohmann::json& json)
{
    auto unzipped = nlohmann::json::object({ { "keys", {} }, { "values", {} } });
    for (auto& [key, value] : json.items()) {
        unzipped["keys"].push_back(key);
        unzipped["values"].push_back(value);
    }
    return unzipped;
}

nlohmann::json json::minimal_diff(const nlohmann::json& source, const nlohmann::json& target, const std::string& path)
{
    auto result = nlohmann::json::array();

    if (source == target) {
        return result;
    }

    // different types: replace value
    if (source.type() != target.type()) {
        result.push_back(
            { { "op", "replace" }, { "path", path }, { "value", target } });
        return result;
    }

    switch (source.type()) {
    case nlohmann::json::value_t::array: {
        // Apply the Needleman-Wunsch algorithm to find the best alignment between the two arrays
        // and therefore find a minimal patch.
        // https://medium.com/@amithunjha/needleman-wunsch-algorithm-for-dna-sequence-alignment-b103b8454de0

        enum class TraversalOp {
            diagonal_match,
            diagonal_mismatch,
            up,
            left,
            done,
        };

        // Initialize the score and traversal matrices
        size_t sourceSize = source.size();
        size_t targetSize = target.size();
        std::vector<std::vector<int>> C(sourceSize + 1, std::vector<int>(targetSize + 1)); // score matrix
        std::vector<std::vector<TraversalOp>> B(sourceSize + 1, std::vector<TraversalOp>(targetSize + 1));
        for (size_t i = 0; i < sourceSize + 1; ++i) {
            C[i][0] = -static_cast<int>(i);
            B[i][0] = TraversalOp::up;
        }
        for (size_t j = 0; j < targetSize + 1; ++j) {
            C[0][j] = -static_cast<int>(j);
            B[0][j] = TraversalOp::left;
        }
        B[0][0] = TraversalOp::done;

        // Calculate scores and and fill matrices
        for (size_t i = 1; i < sourceSize + 1; ++i) {
            for (size_t j = 1; j < targetSize + 1; ++j) {
                // since matrix is one row/ column bigger than source/ target we decrease the counter by one here
                bool match = source[i - 1] == target[j - 1];

                int diagScore = C[i - 1][j - 1] + (match ? 1 : -1);
                int upScore = C[i - 1][j] - 1;
                int leftScore = C[i][j - 1] - 1;
                if (diagScore >= upScore && diagScore >= leftScore) {
                    C[i][j] = diagScore;
                    B[i][j] = match ? TraversalOp::diagonal_match : TraversalOp::diagonal_mismatch;
                } else if (upScore > diagScore && upScore >= leftScore) {
                    C[i][j] = upScore;
                    B[i][j] = TraversalOp::up;
                } else {
                    C[i][j] = leftScore;
                    B[i][j] = TraversalOp::left;
                }
            }
        }

        // Traceback minimal diff and add it to the results

        int i = static_cast<int>(sourceSize);
        int j = static_cast<int>(targetSize);
        while (B[i][j] != TraversalOp::done) {
            if (B[i][j] == TraversalOp::diagonal_match) {
                i--;
                j--;
                continue;
            }
            if (B[i][j] == TraversalOp::diagonal_mismatch) {
                auto diffResult = json::minimal_diff(source[i - 1], target[j - 1], path + "/" + std::to_string(i - 1));
                std::copy(diffResult.begin(), diffResult.end(), std::back_inserter(result));
                i--;
                j--;
                continue;
            }
            if (B[i][j] == TraversalOp::up) {
                result.push_back(nlohmann::json::object(
                    { { "op", "remove" }, { "path", path + "/" + std::to_string(i - 1) } }));
                i--;
                continue;
            }
            if (B[i][j] == TraversalOp::left) {
                result.push_back(nlohmann::json::object(
                    { { "op", "add" }, { "path", path + "/" + std::to_string(i) }, { "value", target[j - 1] } }));
                j--;
                continue;
            }
        }
        break;
    }

    case nlohmann::json::value_t::object: {
        // first pass: traverse this object's elements
        for (auto it = source.cbegin(); it != source.cend(); ++it) {
            // escape the key name to be used in a JSON patch
            const auto path_key = path + "/" + nlohmann::detail::escape(it.key());

            if (target.find(it.key()) != target.end()) {
                // recursive call to compare object values at key it
                auto temp_diff = minimal_diff(it.value(), target[it.key()], path_key);
                result.insert(result.end(), temp_diff.begin(), temp_diff.end());
            } else {
                // found a key that is not in o -> remove it
                result.push_back(nlohmann::json::object(
                    { { "op", "remove" }, { "path", path_key } }));
            }
        }

        // second pass: traverse other object's elements
        for (auto it = target.cbegin(); it != target.cend(); ++it) {
            if (source.find(it.key()) == source.end()) {
                // found a key that is not in this -> add it
                const auto path_key = path + "/" + nlohmann::detail::escape(it.key());
                result.push_back(
                    { { "op", "add" }, { "path", path_key },
                        { "value", it.value() } });
            }
        }

        break;
    }

    case nlohmann::json::value_t::null:
    case nlohmann::json::value_t::string:
    case nlohmann::json::value_t::boolean:
    case nlohmann::json::value_t::number_integer:
    case nlohmann::json::value_t::number_unsigned:
    case nlohmann::json::value_t::number_float:
    case nlohmann::json::value_t::binary:
    case nlohmann::json::value_t::discarded:
    default: {
        // both primitive type: replace value
        result.push_back(
            { { "op", "replace" }, { "path", path }, { "value", target } });
        break;
    }
    }

    return result;
}

nlohmann::json json::filter(nlohmann::json&& json, const std::function<bool(const nlohmann::json&)> predicate)
{
    auto it = json.begin();
    while (it != json.end()) {
        if (!predicate(*it)) {
            it = json.erase(it);
        } else {
            it++;
        }
    }
    return json;
}

nlohmann::json json::flatten_on(const nlohmann::json& json, const std::string& pointerRegex)
{
    nlohmann::json result = nlohmann::json::object();
    std::function<void(const nlohmann::json::json_pointer&)> recursiveFlatten;
    recursiveFlatten = [&](const nlohmann::json::json_pointer& pointer) {
        if (json.contains(pointer) == false) {
            return;
        }

        for (const auto& item : json[pointer].items()) {
            for_each_match(pointerRegex, json[pointer], [&](const nlohmann::json::json_pointer& match) {
                result[pointer.to_string() + match.to_string()] = json[pointer][match];
            });
            recursiveFlatten(pointer / item.key());
        }
    };
    recursiveFlatten(""_json_pointer);
    return result;
}

nlohmann::json json::merge(const nlohmann::json& jsonBase, const nlohmann::json& jsonMerge, bool ignoreConflicts)
{
    nlohmann::json result = jsonMerge.flatten();
    nlohmann::json tmp = jsonBase.flatten();

    for (const auto& [key, value] : tmp.items()) {
        if (result.contains(key) && !ignoreConflicts) {
            throw std::runtime_error("conflicting keys: " + std::string(key));
        }

        result[key] = value;
    }

    return result.unflatten();
}

nlohmann::json json::resolveAllOf(nlohmann::json json)
{
    std::vector<std::string> allOfPointers;
    auto filter = [&allOfPointers](const nlohmann::json& json, const nlohmann::json::json_pointer& pointer) -> void {
        if (!pointer.empty() && pointer.back() == "allOf") {
            allOfPointers.push_back(pointer.to_string());
        }
    };
    std::function<void(const nlohmann::json& json, const nlohmann::json::json_pointer& pointer, decltype(filter) f)> recursiveTraverse;
    recursiveTraverse = [&recursiveTraverse](const nlohmann::json& json, const nlohmann::json::json_pointer& pointer, auto f) -> void {
        if (json.contains(pointer) == false) {
            return;
        }
        f(json, pointer);

        for (const auto& item : json[pointer].items()) {
            recursiveTraverse(json, pointer / item.key(), f);
        }
    };

    recursiveTraverse(json, ""_json_pointer, filter);

    // start with resolving deep objects so that following pointers remain valid
    std::sort(allOfPointers.begin(), allOfPointers.end(),
        [](const std::string& a, const std::string& b) -> bool {
            size_t countA = std::count_if(a.begin(), a.end(), [](char c) { return c == '/'; });
            size_t countB = std::count_if(b.begin(), b.end(), [](char c) { return c == '/'; });
            return countA > countB;
        });

    auto substituteAllOf = [](nlohmann::json json) -> nlohmann::json {
        if (!json.is_object()) {
            return json;
        }

        if (json.contains("allOf")) {
            auto allOf = json["allOf"];
            json.erase("allOf");
            for (auto allOfItem : allOf) {
                json = json::merge(json, allOfItem);
            }
        }
        return json;
    };

    for (auto& item : allOfPointers) {
        auto jsonObject = json.at(nlohmann::json::json_pointer(item).parent_pointer());
        json.at(nlohmann::json::json_pointer(item).parent_pointer()) = substituteAllOf(jsonObject);
    }

    return json;
}

size_t json::findNth(const std::string& string, const std::string& searchString, size_t nth, size_t pos = 0)
{
    if (nth == 0) {
        return std::string::npos;
    }
    size_t searchStringLength = searchString.size();
    for (size_t i = 0; i < nth - 1; ++i) {
        pos = string.find(searchString, pos);
        if (pos == std::string::npos) {
            return std::string::npos;
        }
        pos += 1 + searchStringLength;
    }
    return string.find(searchString, pos);
}

size_t json::pointerLength(const nlohmann::json::json_pointer& pointer)
{
    nlohmann::json::json_pointer copy = pointer;
    size_t result = 0;
    while (!copy.empty()) {
        ++result;
        copy.pop_back();
    }
    return result;
}

nlohmann::json::json_pointer json::getPointerPrefix(const nlohmann::json::json_pointer& pointer, size_t numberOfElements)
{
    if (numberOfElements == 0) {
        return nlohmann::json::json_pointer();
    }
    nlohmann::json::json_pointer result = pointer;
    for (size_t i = pointerLength(pointer); i > numberOfElements; --i) {
        result.pop_back();
    }
    return result;
}

std::vector<std::string> json::findValue(const nlohmann::json& json, nlohmann::json value)
{
    std::vector<std::string> valuePaths;
    auto filter = [&valuePaths, &value](const nlohmann::json& json, const nlohmann::json::json_pointer& pointer) -> void {
        if (json.at(pointer) == value) {
            valuePaths.push_back(pointer.to_string());
        }
    };
    std::function<void(const nlohmann::json& json, const nlohmann::json::json_pointer& pointer, decltype(filter) f)> recursiveTraverse;
    recursiveTraverse = [&recursiveTraverse](const nlohmann::json& json, const nlohmann::json::json_pointer& pointer, auto f) -> void {
        if (json.contains(pointer) == false) {
            return;
        }
        f(json, pointer);

        for (const auto& item : json[pointer].items()) {
            recursiveTraverse(json, pointer / item.key(), f);
        }
    };

    recursiveTraverse(json, ""_json_pointer, filter);

    return valuePaths;
}

bool json::arrayContains(const nlohmann::json& json, const nlohmann::json& value)
{
    if (!json.is_array()) {
        return false;
    }

    return std::find(json.begin(), json.end(), value) != json.end();
}

void json::erase(nlohmann::json& json, nlohmann::json::json_pointer path)
{
    if (path.empty()) {
        return;
    }

    auto target = path.back();
    path.pop_back();
    json.at(path).erase(target);
}
nlohmann::json json::tree_diff(const nlohmann::json& source, const nlohmann::json& target, const std::string& childrenKey)
{
    auto result = nlohmann::json::array();

    if (source == target) {
        return result;
    }

    if (source.type() != nlohmann::json::object() || target.type() != nlohmann::json::object()) {
        throw std::invalid_argument("source and target must be json objects");
    }

    std::function<void(const nlohmann::json&, const nlohmann::json::json_pointer&)> recursive_add_children;
    recursive_add_children = [&recursive_add_children, &result, &childrenKey](const nlohmann::json& targetObject, const nlohmann::json::json_pointer& objectPath) {
        nlohmann::json target = nlohmann::json::array();
        if (targetObject.contains(childrenKey)) {
            target = targetObject[childrenKey];
        }

        for (size_t i = 0; i < target.size(); ++i) {
            auto value = target[i];
            value.erase(childrenKey);
            auto childPath = objectPath / childrenKey / i;
            result.push_back(nlohmann::json::object(
                {
                    { "op", "add" },
                    { "path", childPath.to_string() },
                    { "value", value },
                }));
            recursive_add_children(target[i], childPath);
        }
    };

    std::function<void(const nlohmann::json&, const nlohmann::json::json_pointer&)> recursive_remove_children;
    recursive_remove_children = [&recursive_remove_children, &result, &childrenKey](const nlohmann::json& sourceObject, const nlohmann::json::json_pointer& objectPath) {
        nlohmann::json source = nlohmann::json::array();
        if (sourceObject.contains(childrenKey)) {
            source = sourceObject[childrenKey];
        }

        for (size_t i = source.size(); i > 0; --i) {
            auto childPath = objectPath / childrenKey / (i - 1);
            recursive_remove_children(source[(i - 1)], childPath);
            result.push_back(nlohmann::json::object(
                {
                    { "op", "remove" },
                    { "path", childPath.to_string() },
                }));
        }
    };

    auto compare_values = [childrenKey](nlohmann::json first, nlohmann::json second) {
        first.erase(childrenKey);
        second.erase(childrenKey);
        return first == second;
    };

    // Recursive function that acts on children arrays
    std::function<void(const nlohmann::json& source, const nlohmann::json& target, const nlohmann::json::json_pointer& path)> recursive_tree_diff;
    recursive_tree_diff = [&recursive_tree_diff, &recursive_add_children, &recursive_remove_children, &result, &childrenKey, &compare_values](const nlohmann::json& sourceObject, const nlohmann::json& targetObject, const nlohmann::json::json_pointer& objectPath) {
        // Sanitize inputs
        nlohmann::json source = nlohmann::json::array();
        nlohmann::json target = nlohmann::json::array();
        if (sourceObject.contains(childrenKey)) {
            source = sourceObject[childrenKey];
        }
        if (targetObject.contains(childrenKey)) {
            target = targetObject[childrenKey];
        }
        const auto& path = objectPath / childrenKey;

        // Apply the Needleman-Wunsch algorithm to find the best alignment between the two arrays
        // and therefore find a minimal patch.
        // https://medium.com/@amithunjha/needleman-wunsch-algorithm-for-dna-sequence-alignment-b103b8454de0

        enum class TraversalOp {
            diagonal_match,
            diagonal_mismatch,
            up,
            left,
            done,
        };

        // Initialize the score and traversal matrices
        size_t sourceSize = source.size();
        size_t targetSize = target.size();
        std::vector<std::vector<int>> C(sourceSize + 1, std::vector<int>(targetSize + 1)); // score matrix
        std::vector<std::vector<TraversalOp>> B(sourceSize + 1, std::vector<TraversalOp>(targetSize + 1));
        for (size_t i = 0; i < sourceSize + 1; ++i) {
            C[i][0] = -static_cast<int>(i);
            B[i][0] = TraversalOp::up;
        }
        for (size_t j = 0; j < targetSize + 1; ++j) {
            C[0][j] = -static_cast<int>(j);
            B[0][j] = TraversalOp::left;
        }
        B[0][0] = TraversalOp::done;

        // Calculate scores and and fill matrices
        for (size_t i = 1; i < sourceSize + 1; ++i) {
            for (size_t j = 1; j < targetSize + 1; ++j) {
                // since matrix is one row/ column bigger than source/ target we decrease the counter by one here
                bool match = source[i - 1] == target[j - 1];

                int diagScore = C[i - 1][j - 1] + (match ? 1 : -1);
                int upScore = C[i - 1][j] - 1;
                int leftScore = C[i][j - 1] - 1;
                if (diagScore >= upScore && diagScore >= leftScore) {
                    C[i][j] = diagScore;
                    B[i][j] = match ? TraversalOp::diagonal_match : TraversalOp::diagonal_mismatch;
                } else if (upScore > diagScore && upScore >= leftScore) {
                    C[i][j] = upScore;
                    B[i][j] = TraversalOp::up;
                } else {
                    C[i][j] = leftScore;
                    B[i][j] = TraversalOp::left;
                }
            }
        }

        // Traceback minimal diff and add it to the results
        int i = static_cast<int>(sourceSize);
        int j = static_cast<int>(targetSize);

        enum class Alignment {
            Match,
            Mismatch,
            Insertion,
            Deletion
        };
        struct Patch {
            Alignment alignment;
            int i;
            int j;
        };
        std::queue<Patch> alignmentSourceToTarget;

        while (B[i][j] != TraversalOp::done) {
            if (B[i][j] == TraversalOp::diagonal_match) {
                alignmentSourceToTarget.push({ Alignment::Match, i - 1, j - 1 });
                i--;
                j--;
                continue;
            }
            if (B[i][j] == TraversalOp::diagonal_mismatch) {
                alignmentSourceToTarget.push({ Alignment::Mismatch, i - 1, j - 1 });
                i--;
                j--;
                continue;
            }
            if (B[i][j] == TraversalOp::up) {
                alignmentSourceToTarget.push({ Alignment::Deletion, i - 1, j });
                i--;
                continue;
            }
            if (B[i][j] == TraversalOp::left) {
                alignmentSourceToTarget.push({ Alignment::Insertion, i, j - 1 });
                j--;
                continue;
            }
        }

        // Create a json patch from the alignment (including patch operations for all children)
        while (!alignmentSourceToTarget.empty()) {
            auto patch = alignmentSourceToTarget.front();
            alignmentSourceToTarget.pop();
            auto diffPointer = path / patch.i;
            switch (patch.alignment) {
            case Alignment::Match:
                break;
            case Alignment::Mismatch:
                if (compare_values(source[patch.i], target[patch.j])) {
                    recursive_tree_diff(source[patch.i], target[patch.j], diffPointer);
                } else {
                    // Replace the whole subtree
                    recursive_remove_children(source[patch.i], diffPointer);
                    result.push_back(nlohmann::json::object(
                        {
                            { "op", "remove" },
                            { "path", diffPointer.to_string() },
                        }));
                    auto value = target[patch.j];
                    value.erase(childrenKey);
                    result.push_back(nlohmann::json::object(
                        {
                            { "op", "add" },
                            { "path", diffPointer.to_string() },
                            { "value", value },
                        }));
                    recursive_add_children(target[patch.j], diffPointer);
                }

                break;
            case Alignment::Deletion:
                recursive_remove_children(source[patch.i], diffPointer);
                result.push_back(nlohmann::json::object(
                    {
                        { "op", "remove" },
                        { "path", diffPointer.to_string() },
                    }));
                break;
            case Alignment::Insertion:
                auto value = target[patch.j];
                value.erase(childrenKey);
                result.push_back(nlohmann::json::object(
                    {
                        { "op", "add" },
                        { "path", diffPointer.to_string() },
                        { "value", value },
                    }));
                recursive_add_children(target[patch.j], diffPointer);
                break;
            }
        }
    };

    recursive_tree_diff(source, target, nlohmann::json::json_pointer());

    return result;
}
