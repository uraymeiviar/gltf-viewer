// Copyright © 2022 UX3D GmbH. All rights reserved.

#include "operations/Library.h"
#include "reactor/Json.h"
#include "reactor/Operations.h"
#include "reactor/Schema.h"
#include <gltf_schema/gltf_single_json.h>

#include <fstream>
#include <iostream>

using namespace ux3d;
using namespace ux3d::operations;

int main()
{

    Library library;

    const nlohmann::json gltfSchema = reactor::schema::resolveRefs(gltf_schema::merged(), "glTF.schema.json");
    const nlohmann::json linearizedSchema = reactor::schema::linearize(gltfSchema);
    library.loadPlugin(ux3d::operations::gltfOperations(gltfSchema, linearizedSchema));
    nlohmann::json byName = reactor::operations::byName(library.schema);
    nlohmann::json byPathPattern = reactor::operations::byPathPattern(byName);
    {
        const std::string operationsSchema = library.schema.dump(2);
        std::ofstream jsonFile;
        jsonFile.open("operations_schema.json");
        jsonFile << operationsSchema;
        jsonFile.close();
    }

    {
        std::ofstream jsonFile;
        jsonFile.open("operations_byName.json");
        jsonFile << byName.dump(2);
        jsonFile.close();
    }

    {
        std::ofstream jsonFile;
        jsonFile.open("operations_byPathPattern.json");
        jsonFile << byPathPattern.dump(2);
        jsonFile.close();
    }

    {
        std::ofstream jsonFile;
        jsonFile.open("gltf_schema.json");
        jsonFile << gltfSchema.dump(2);
        jsonFile.close();
    }
    {
        auto jsonResolved = reactor::json::resolveAllOf(gltfSchema);
        std::ofstream jsonFile;
        jsonFile.open("gltf_schema_resolved.json");
        jsonFile << jsonResolved.dump(2);
        jsonFile.close();
    }
    {
        std::ofstream jsonFile;
        jsonFile.open("linearized_schema.json");
        jsonFile << linearizedSchema.dump(2);
        jsonFile.close();
    }
    return 0;
}
