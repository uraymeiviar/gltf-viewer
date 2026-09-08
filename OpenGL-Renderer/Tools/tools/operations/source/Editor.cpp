// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "Editor.h"
#include "operations/Operations.h"
#include "operations/Schema.h"

#include <string>

using namespace ux3d;
using namespace ux3d::operations;

const Operation editor::addSelection = {
    .name = "editor.addSelection",

    .schema = schema::operation(
        "Add to selection",
        "Adds the provided json path to the selection. Moves it to front if already selected. First object is always the latest selection."
        "cannot check the validity of the selection.",
        schema::object(
            { schema::parameter::uuid(), { "localPath", schema::pointer(".*") } })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto uuid = params["uuid"];
        auto localPath = params["localPath"];

        if (state.contains("selection") == false) {
            state["selection"] = nlohmann::json::array();
        }

        nlohmann::json tmpObj;
        tmpObj["uuid"] = uuid;
        tmpObj["localPath"] = localPath;

        // erase pointer if already contained
        // std::find has to be used since nlohmann::json::contains only works for json objects and not arrays
        auto itr = std::find(state["selection"].begin(), state["selection"].end(), tmpObj);
        if (itr != state["selection"].end()) {
            state["selection"].erase(itr);
        }
        state["selection"].insert(state["selection"].begin(), tmpObj); // insert at front of array
        return state;
    }
};

const Operation editor::setSelection = {
    .name = "editor.setSelection",

    .schema = schema::operation(
        "Sets the selection",
        "Sets the provided json path as the selection. Clears selection first. Then sets new selection"
        "cannot check the validity of the selection.",
        schema::array(schema::object({ schema::parameter::uuid(), { "localPath", schema::pointer(".*") } }), 0)),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        state["selection"] = params;
        return state;
    }
};

const Operation editor::removeSelection = {
    .name = "editor.removeSelection",

    .schema = schema::operation(
        "Remove from selection",
        "Removes json pointer from selection."
        "cannot check the validity of the value.",
        schema::object(
            { schema::parameter::uuid(), { "localPath", schema::pointer(".*") } })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto uuid = params["uuid"];
        auto localPath = params["localPath"];

        if (state.contains("selection") == false) {
            state["selection"] = nlohmann::json::array();
            return state;
        }

        nlohmann::json tmpObj;
        tmpObj["uuid"] = uuid;
        tmpObj["localPath"] = localPath;

        // erase pointer if contained
        // std::find has to be used since nlohmann::json::find only works for json objects and not arrays
        auto itr = std::find(state["selection"].begin(), state["selection"].end(), tmpObj);
        if (itr != state["selection"].end()) {
            state["selection"].erase(itr);
        }

        return state;
    }
};

const Operation editor::clearSelection = {
    .name = "editor.clearSelection",

    .schema = schema::operation(
        "Clear selection",
        "Clears the selection."
        "cannot check the validity of the value.",
        schema::object({})),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (state.contains("selection") == false) {
            state["selection"] = nlohmann::json::array();
            return state;
        }

        state["selection"].clear();
        return state;
    }
};

const Operation editor::toggleWireframe = {
    .name = "editor.toggleWireframe",

    .schema = schema::operation(
        "Toggle wireframe",
        "Toggles wireframe mode for the provided mesh. if mesh in wireframe -> returned array state['wireframe'] contains path to mesh"
        "cannot check the validity of the value.",
        schema::object({ schema::parameter::uuid() })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (state.contains("wireframe") == false) {
            state["wireframe"] = nlohmann::json::array();
        }

        auto uuid = params["uuid"];

        // check if path already contained
        auto itr = std::find(state["wireframe"].begin(), state["wireframe"].end(), uuid);
        // if contained remove it
        if (itr != state["wireframe"].end()) {
            state["wireframe"].erase(itr);
        } else {
            // if not contained add path
            state["wireframe"].push_back(uuid);
        }

        return state;
    }
};

const Operation editor::toggleVisibility = {
    .name = "editor.toggleVisibility",

    .schema = schema::operation(
        "Toggle visibility",
        "Toggles visibility mode for the provided mesh. if mesh in visibility -> returned array state['visibility'] contains path to mesh"
        "cannot check the validity of the value.",
        schema::object({ schema::parameter::uuid() })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (state.contains("visibility") == false) {
            state["visibility"] = nlohmann::json::array();
        }

        auto uuid = params["uuid"];

        // check if path already contained
        auto itr = std::find(state["visibility"].begin(), state["visibility"].end(), uuid);
        // if contained remove it
        if (itr != state["visibility"].end()) {
            state["visibility"].erase(itr);
        } else {
            // if not contained add path
            state["visibility"].push_back(uuid);
        }

        return state;
    }
};
