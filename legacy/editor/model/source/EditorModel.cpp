// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "model/EditorModel.h"
#include "core/Logger.h"
#include "model/GltfModel.h"
#include "operations/Schema.h"
#include "reactor/Gltf.h"
#include "reactor/Json.h"
#include "reactor/Reactor.h"
#include "reactor/Schema.h"

#include <gltf_schema/gltf_single_json.h>

#include <functional>
#include <iostream>

using namespace ux3d;
using namespace ux3d::model;

Editor::Editor(model::Gltf& gltf)
{
    auto schemas = gltf_schema::merged();
    auto gltfSchema = reactor::schema::resolveRefs(gltf_schema::merged(), "glTF.schema.json");
    auto linearizedSchema = reactor::schema::linearize(gltfSchema);
    m_operationsLibrary.loadPlugin(operations::editorOperations());

    head = state.get_observable()
               .with_latest_from(gltf.uuidPaths, gltf.head)
               .map([=](const std::tuple<nlohmann::json, nlohmann::json, nlohmann::json>& tuple) {
                   nlohmann::json editorState = std::get<0>(tuple);
                   const nlohmann::json& uuids = std::get<1>(tuple);
                   const nlohmann::json& gltf = std::get<2>(tuple)["gltf"];

                   // clean selection, wireframe and hide arrays of all elements that no longer exist.

                   try {
                       if (editorState.contains("selection")) {
                           nlohmann::json& selection = editorState["selection"];
                           for (size_t i = 0; i < selection.size(); ++i) {
                               const std::string& localPath = selection.at(i).at("localPath");
                               // if parent uuid (or own in case local path is "/") was deleted
                               if (!uuids.contains(selection.at(i).at("uuid"))) {
                                   selection.erase(i);
                                   i--;
                               } else if (!localPath.empty()) {
                                   // check if actual element was deleted
                                   const std::string& parentUUID = selection.at(i).at("uuid");
                                   const std::string& parentPath = uuids.at(parentUUID);
                                   const nlohmann::json::json_pointer completePath(parentPath + localPath);
                                   if (gltf.contains(completePath) && gltf.at(completePath).is_string()) {
                                       std::string referencedPath = gltf.at(completePath);
                                       const nlohmann::json referencedProperty = gltf.at(nlohmann::json::json_pointer(referencedPath));
                                       if (referencedProperty.contains("uuid") && !uuids.contains(referencedProperty.at("uuid"))) {
                                           selection.erase(i);
                                           i--;
                                       }
                                   }
                               }
                           }
                       }
                   } catch (const std::exception& e) {
                       core::logError(e.what());
                   }

                   if (editorState.contains("wireframe")) {
                       nlohmann::json& wireframe = editorState["wireframe"];
                       for (size_t i = 0; i < wireframe.size(); ++i) {
                           if (!uuids.contains(wireframe[i])) {
                               wireframe.erase(i);
                               i--;
                           }
                       }
                   }

                   if (editorState.contains("visibility")) {
                       nlohmann::json& visibility = editorState["visibility"];
                       for (size_t i = 0; i < visibility.size(); ++i) {
                           if (!uuids.contains(visibility[i])) {
                               visibility.erase(i);
                               i--;
                           }
                       }
                   }

                   return editorState;
               })
               .publish()
               .ref_count();

    headShared = head.map([](const nlohmann::json& head) {
                         return std::make_shared<const nlohmann::json>(head);
                     })
                     .publish(std::make_shared<const nlohmann::json>(nlohmann::json()))
                     .ref_count()
                     .filter([](const auto& json) { return !json->is_null(); });

    // Create observable for the current head of mementos
    diff = head
               .start_with(nlohmann::json::object())
               .pairwise()
               .map([](const std::tuple<nlohmann::json, nlohmann::json>& tuple) {
                   const nlohmann::json& first = std::get<0>(tuple);
                   nlohmann::json diff = nlohmann::json::diff(first, std::get<1>(tuple));

                   if (!diff.empty()) {
                       diff[0]["prevState"] = first;
                   }
                   return diff;
               })
               .publish()
               .ref_count();

    auto individualDiff = diff
                              .concat_map([](const nlohmann::json& diffs) {
                                  return rxcpp::observable<>::create<nlohmann::json>([&diffs](const rxcpp::subscriber<nlohmann::json>& s) {
                                      for (auto& diff : diffs) {
                                          s.on_next(diff);
                                      }
                                      s.on_completed();
                                  });
                              })
                              .publish()
                              .ref_count()
                              .filter([](const nlohmann::json& patch) { return patch.contains("path"); });

    wireframe = individualDiff.filter([](const nlohmann::json& patch) {
        return patch["path"].get<std::string>().find("wireframe") != std::string::npos;
    });
    visibility = individualDiff.filter([](const nlohmann::json& patch) {
        return patch["path"].get<std::string>().find("visibility") != std::string::npos;
    });

    selection = individualDiff
                    .filter([](const nlohmann::json& patch) {
                        return patch["path"].get<std::string>().find("selection") != std::string::npos;
                    })
                    .with_latest_from(head, gltf.head, gltf.uuidPaths)
                    .map([](const std::tuple<nlohmann::json, nlohmann::json, nlohmann::json, nlohmann::json>& tuple) {
                        const auto& editorHead = std::get<1>(tuple);
                        const auto& gltfHead = std::get<2>(tuple);
                        const auto& uuidPath = std::get<3>(tuple);

                        if (!editorHead.contains("selection") || !editorHead["selection"].is_array()) {
                            return nlohmann::json::array();
                        }

                        auto selection = editorHead["selection"];
                        for (auto& selected : selection) {
                            std::string elementUUID = selected["uuid"];
                            const std::string& localPath = selected["localPath"];
                            std::string referencePath;
                            std::string elementAbsolutePath = "";
                            if (!localPath.empty()) {
                                const std::string& parentPath = uuidPath.at(elementUUID);
                                nlohmann::json::json_pointer completePathPointer(parentPath + localPath);
                                referencePath = completePathPointer.to_string();
                                if (gltfHead.at("gltf").contains(completePathPointer) && gltfHead.at("gltf").at(completePathPointer).is_string()) {
                                    std::string resolvedPath = gltfHead.at("gltf").at(completePathPointer);
                                    if (const auto& referencedProperty = gltfHead.at("gltf").at(nlohmann::json::json_pointer(resolvedPath)); referencedProperty.contains("uuid")) {
                                        elementUUID = referencedProperty["uuid"];
                                    }
                                } else {
                                    elementUUID = "";
                                    elementAbsolutePath = referencePath;
                                }
                            } else {
                                referencePath = uuidPath.at(elementUUID);
                            }
                            selected["referencePath"] = referencePath;
                            selected["elementUUID"] = elementUUID;
                            selected["elementAbsolutPath"] = elementAbsolutePath.empty() ? uuidPath[elementUUID].get<std::string>() : elementAbsolutePath;
                        }
                        return selection;
                    })
                    .start_with(nlohmann::json::array())
                    .publish()
                    .ref_count();

    m_operations.get_observable()
        .with_latest_from(state.get_observable())
        .subscribe([this](const std::tuple<nlohmann::json, nlohmann::json>& tuple) {
            const nlohmann::json& operation = std::get<0>(tuple);
            nlohmann::json state = std::get<1>(tuple);
            try {
                datastorage::DataStorage storage;
                auto futureState = m_operationsLibrary.call(operation, std::move(state), storage, [](auto&&...) {});
                auto diff = nlohmann::json::diff(state, futureState);
                this->state.get_subscriber().on_next(futureState);
            } catch (const std::exception& e) {
                core::logError(e.what());
            } }, [](std::exception_ptr ep) {
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception& ex) {
                core::logError("%s", ex.what());
            } });
}

void Editor::run(const nlohmann::json& operation)
{
    m_operations.get_subscriber().on_next(operation);
}
