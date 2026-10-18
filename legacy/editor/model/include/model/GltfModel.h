
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

using Mementos = mementos::Mementos<nlohmann::json>;

/**
 * @brief Model for a glTF state
 *
 */
class Gltf {
    friend class Editor;

public:
    static rxcpp::observable<nlohmann::json> observeOnSameThread(const rxcpp::observable<nlohmann::json>& observable) { return observable; }
    static rxcpp::observable<nlohmann::json> observeOnNewThread(const rxcpp::observable<nlohmann::json>& observable) { return observable.observe_on(rxcpp::observe_on_new_thread()); }

    Gltf(const std::function<rxcpp::observable<nlohmann::json>(const rxcpp::observable<nlohmann::json>&)>& observeOn = &Gltf::observeOnSameThread);

    /**
     * @brief Schedule a task for execution in the operations thread.
     * @param task task description, the schema of which is available from the library
     */
    DispatchQueue::ResultPipes schedule(const nlohmann::json& task);

    void loadPlugin(const std::vector<operations::Operation>& pluginOperations);

    /**
     * @brief Apply a glTF state to the model. This will commit the new state to mementos.
     * @param gltfState new state to be applied.
     */
    void apply(nlohmann::json&& gltfState);

    /**
     * @brief Clear the mementos
     */
    void clearHistory();

    /**
     * @brief Undo the last commit to mementos
     */
    void undo();

    /**
     * @brief Redo the last undone commit in mementos
     */
    void redo();

    /**
     * @brief The `head` observable allows to subscribe to commits and other changes of the
     * glTF state at the head of mementos. Changes can come from operations being executed on
     * the state or undo / redo steps that move the current head to past or future states.
     */
    rxcpp::observable<nlohmann::json> head;

    /**
     * @brief Similar to the `head` observable but the head is stored in a shared pointer so that
     * unnecessary copies of the JSON can be avoided
     */
    rxcpp::observable<std::shared_ptr<const nlohmann::json>> headShared;

    /**
     * @brief Emits observables for each new gltf that each contain the head changes until the next
     * new gltf.
     *  {@code
     *      newGltf: ----1----------------2----------|->
     *      head:    ----A-B-C-D----------E-F-G------|->
     *      [ head.window_toggle(newGltfIndex, newGltfIndex.skip(1)) ]
     *               ----\----------------\----------|->
     *                    A                E
     *                     B                F
     *                      C                G
     *                       D                -------|->
     *                        ------------|->
     *  }
     */
    rxcpp::observable<rxcpp::observable<nlohmann::json>> headWindowed;

    /**
     * @brief Similar to the `head` observable but taken directly from the mementos state without
     * any transformations to the data (like e.g. replacing index references with pointers)
     */
    rxcpp::observable<nlohmann::json> headRaw;

    /**
     * @brief The `validation` observable contains a report from the glTF-Validator
     */
    rxcpp::observable<nlohmann::json> validation;

    /**
     * @brief The `hierarchy` observable allows to subscribe to a view on the current glTF
     * state that organizes all glTF properties in a hierarchy. This hierarchy is based on the
     * references within glTF, e.g. The Asset is at the root with Animations and Scenes as
     * direct children.
     */
    rxcpp::observable<nlohmann::json> hierarchy;

    /**
     * @brief The `resources` observable allows to subscribe to a view on the current glTF
     * state that organizes all glTF properties in a ressource view.
     * It is a different representation of the root gltf object, readable by the model of the tree view.
     */
    rxcpp::observable<nlohmann::json> resources;

    /**
     * @brief The `diffs` observable emits observables containing all diffs since the last
     *  `newGltfIndex`. Each of these emitted observables terminates when the next `newGltfIndex` occurs
     *  {@code
     *      newGltf: ----1----------------2----------|->
     *      head:    ----A-B-C-D----------E-F-G------|->
     *      [ head.window_toggle(newGltfIndex, newGltfIndex.skip(1)) ]
     *               ----\----------------\----------|->
     *                    A                E
     *                     B                F
     *                      C                G
     *                       D                -------|->
     *                        ------------|->
     *      [ map(o -> o.startWith({}).pairwise().map(x, y -> diff(x, y)) ]
     *  }
     */
    rxcpp::observable<rxcpp::observable<nlohmann::json>> diffs;

    rxcpp::observable<nlohmann::json> binDiff;
    rxcpp::observable<std::tuple<nlohmann::json, nlohmann::json>> binDiffComb;
    /**
     * @brief The `uuids` contains uuids for all objects of the current head gltf. The UUIDs
     *  are reset when a new glTF is loaded and are updated for all changes to the glTF head.
     *  {@code
     *      diffs:   ----\----------------\----------|->
     *                    A                E
     *                      B                F
     *                        C                G
     *                          D               -----|->
     *                            --------|->
     *      [ flat_map(
     *                   -A-B-C-D---------|->
     *                   [ scan(uuids, diff -> propertyUUIDs(uuids, diff))]
     *                                    -E-F-G-----|->
     *                                    [ scan(uuids, diff -> propertyUUIDs(uuids, diff))]
     *                                                  ]
     *                -----a-b-c-d----------e-f-g----|->
     *  }
     */
    rxcpp::observable<nlohmann::json> uuids;

    /**
     * @brief The `uuidPaths` contains a mapping from all uuids to their json path of the current head gltf
     *
     */
    rxcpp::observable<nlohmann::json> uuidPaths;

    /**
     * @brief The `newGltfIndex` observable allows to subscribe to a newly loaded gltf. For each newly
     * loaded glTF a new integer value is emitted, starting with `1`.
     *
     */
    rxcpp::observable<int> newGltfIndex;

    /**
     * @brief The `operationsByPath` observable allows to subscribe to operations added to the model's operations library
     * where the operations are grouped by the `path` parameter that they act upon
     */
    rxcpp::observable<nlohmann::json> operationsByPath;

    // TODO: make these private. The only reason these are currently public is the StateHistory
    // needing to have full control over mementos
    std::mutex m_mementosMutex;
    mementos::Mementos<nlohmann::json> m_mementos;
    datastorage::DataStorage m_dataStorage;

    rxcpp::observable<bool> canUndo;
    rxcpp::observable<bool> canRedo;

    nlohmann::json operationsSchema;
    nlohmann::json m_linearizedSchema;

    /**
     * @brief An observable containing event reports. This can be used to subscribe to events that are emitted by the operations.
     * The events are emitted as JSON objects with the following structure:
    /**
     * @brief contains image metadata.
     * Each image is represented as a JSON object.
     * Each image is referenced by its uuid.
     *
     * Each image entry has at least the width, height, and mimeType properties.
     *
     * data example:
     *  {
     *      "image uuid": {
     *          "width": 320,
     *          "height": 214,
     *          "mimeType": "image/webp"
     *          "pixelByteSize": 3,
     *          "channels": 3
     *      }, ...
     *  }
     *
     */
    rxcpp::observable<nlohmann::json> imageMetadata;
    /**
     * @brief Contains a mapping from image uuid to an object with either the datastorage id or the bufferview and the datastorage id.
     *
     *
     * data example:
     *
     * {
     *     "1b112651-18fb-4f81-ab4b-17cdbf681fdb": {datastorage: 7},
     *     "1b112651-18fb-4f81-ab4b-17cdbf681fdb": {bufferview: /gltf/bufferViews/0, datastorage: 8},
     *    ...
     * }
     *
     */
    rxcpp::observable<nlohmann::json> imageUUIDToDatastorageID;
    /**
     * @brief Contains a minimal diff with all changes imageUUIDToDatastorageID observable.
     *
     */
    rxcpp::observable<nlohmann::json> imageDiff;

private:
    operations::Library m_operationsLibrary;
    model::DispatchQueue m_operationsDispatchQueue;
};

}
