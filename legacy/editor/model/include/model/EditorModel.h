// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "core/DispatchQueue.h"
#include "datastorage/DataStorage.h"
#include "mementos/Mementos.h"
#include "operations/Library.h"

#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include <rxcpp/rx.hpp>

namespace ux3d::model {

class Gltf;

using Mementos = mementos::Mementos<nlohmann::json>;

/**
 * @brief Model for an editor state
 *
 */
class Editor {
public:
    Editor(model::Gltf&);

    /**
     * @brief Run an operation
     * @param task task description, the schema of which is available from the library
     */
    void run(const nlohmann::json& operation);

    /**
     * @brief The `head` observable allows to subscribe to commits and other changes of the
     * editor state at the head of mementos. Changes can come from operations being executed on
     * the state or undo / redo steps that move the current head to past or future states.
     */
    rxcpp::observable<nlohmann::json> head;

    rxcpp::observable<std::shared_ptr<const nlohmann::json>> headShared;

    /**
     * @brief The `diff` observable allows to subscribe to only the changes to the current head compared
     * to the last
     *
     */
    rxcpp::observable<nlohmann::json> diff;

    /**
     * @brief The `selection` observable allows to subscribe to the current selection
     * A selection looks like this:
     *
     * {
     *      "uuid": "parentUUID",
     *      localPath: "local path relative to parent element (e.g. /camera)",
     *      referencePath: "complete path to reference (e.g. /nodes/2/camera)"
     *      elementUUID: "uuid of actual element",
     *      elementAbsolutPath: "absolut path to element (e.g. /cameras/0)"
     *      type: "Node" type of selected element
     * }
     *
     * if element is not a reference (e.g. a scene is selected) referencePath and elementAbsolutPath are identical
     *
     */
    rxcpp::observable<nlohmann::json> selection;

    /**
     * @brief The `wireframe` observable allows to subscribe to the changes to the wireframe (contains json patches)
     *
     */
    rxcpp::observable<nlohmann::json> wireframe;

    /**
     * @brief The `visibility` observable allows to subscribe to the changes to the visibility (contains json patches)
     *
     */
    rxcpp::observable<nlohmann::json> visibility;

    rxcpp::subjects::subject<nlohmann::json> state;

private:
    /// TODO move operation to own "editor library".
    operations::Library m_operationsLibrary;
    rxcpp::subjects::subject<nlohmann::json> m_operations;
};
}
