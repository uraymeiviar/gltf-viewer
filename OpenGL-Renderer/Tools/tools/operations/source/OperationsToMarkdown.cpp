// Copyright © 2022 UX3D GmbH. All rights reserved.

#include "operations/OperationsToMarkdown.h"

#include "operations/Library.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using namespace ux3d;
using namespace ux3d::operations;
using namespace std::string_literals;

struct ParameterInfo {
    std::string name = "";
    std::string type = "";
    std::string pattern = "";
};

struct OperationInfo {
    std::string callID = "";
    std::string title = "";
    std::string description = "";
    std::vector<ParameterInfo> parameters;
};

ParameterInfo extractParameterInfo(std::string name, nlohmann::json parameter)
{
    ParameterInfo info;
    info.name = name;

    if (parameter.contains("type")) {
        info.type = parameter["type"];
    }
    if (parameter.contains("pattern")) {
        info.pattern = parameter["pattern"];
    }
    return info;
}

OperationInfo extractOperationInfo(std::string callID, nlohmann::json op)
{
    OperationInfo info;
    info.callID = callID;
    if (op.contains("title")) {
        info.title = op["title"];
    }
    if (op.contains("description")) {
        info.description = op["description"];
    }
    info.parameters.clear();
    if (op.contains("properties")) {
        for (auto it : op.at("properties").items()) {
            info.parameters.push_back(extractParameterInfo(it.key(), it.value()));
        }
    }
    return info;
}

void infoToMarkdown(std::string path, std::vector<OperationInfo> operationInfoVector)
{
    std::ofstream file(path, std::ofstream::trunc);

    for (auto op : operationInfoVector) {
        file << "### " << op.title << "\n";
        file << "" << op.description << "\n\n";
        file << " `" << op.callID << "` \n";
        for (auto parameter : op.parameters) {
            file << "-  `" << parameter.name << "` (" << parameter.type << ") " << parameter.pattern << "\n";
        }
        file << "\n\n";
    }
}

void ux3d::operations::utils::extractMarkdown(std::string path, const ux3d::operations::Library& library)
{
    auto operationsArray = library.schema.at("oneOf");
    std::vector<OperationInfo> operationInfoVector;
    for (auto op : operationsArray) {
        for (auto it : op.at("properties").items()) {
            OperationInfo info = extractOperationInfo(it.key(), it.value());
            operationInfoVector.push_back(info);
        }
    }

    infoToMarkdown(path, operationInfoVector);
}
