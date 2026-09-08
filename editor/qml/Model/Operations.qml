pragma Singleton

import "../js/thirdparty.qml.mjs" as Thirdparty;
import "../Components" as Components

import QtQuick 2.15
import Model 1.0 as Model

Item {
    
    function getSetterOperation(operations) {
        if(!operations) {
            return undefined;
        }
        const operationNames = [
            "set_gltf_value",
            "dangerous.setValue"
        ];
        return containsOperation(operations, operationNames);
    }

    function getResetOperation(operations) {
        if(!operations) {
            return undefined;
        }
            const operationNames = [
            "reset",
            "dangerous.removeValue"
        ];
        return containsOperation(operations, operationNames);
    }

    function containsOperation(operations, operationNames) {
        for (const operationName of operationNames) {
            if (operations[operationName]){
                return operationName;
            }
        }
        return undefined;
    }
    
    function execute(operationName, args) {

        let elementPaths = [];
        for(let selectedElement of Model.Observables.selection) {
            elementPaths.push(selectedElement["elementAbsolutPath"])
        }
        if (elementPaths.length > 1){
            let forEachParamters = {}
            if (args.path !== undefined) {
                const propertyPath = args.path.substring(5 + elementPaths[0].length); // /gltf has length 5
                for (let i = 0; i < elementPaths.length; i++) {
                    elementPaths[i] += propertyPath;
                }
            }
            forEachParamters["elementPaths"] = elementPaths;
            forEachParamters["operation"] = operationName;
            forEachParamters["parameters"] = args;
            operationName = "dangerous.forEachElement";
            args = forEachParamters;
        }

        let task = {
            [operationName]: args
        };

        let promise = {
            _fulfillmentCallbacks: [],
            then: function(callback) {
                this._fulfillmentCallbacks.push(callback);
                return this;
            },
            _rejectionCallbacks: [],
            catch: function(callback) {
                this._rejectionCallbacks.push(callback);
                return this;
            },
            fulfill: function(message) {
                this._fulfillmentCallbacks.map((callback) => callback(message));
            },
            reject: function(reason) {
                this._rejectionCallbacks.map((callback) => callback(reason));
            }
        };

        let rejection = (reason) => {
            promise.reject(reason);
        };

        let fulfillment = (message) => {
            promise.fulfill(message);
        };

        applicationData.executeWithCallback(JSON.stringify(task), rejection, fulfillment);

        return promise;
    }

    function runEditorOperation(operationName, args) {
        let task = {
            [operationName]: args
        }
        applicationData.runEditorOperation(JSON.stringify(task));
    }

    function runOperationOnSelection(operationName, parameters = {}){
        if (Model.Observables.selection.length === 0){
            return Promise.reject("Nothing is selected");
        } else if (Model.Observables.selection.length === 1) {
            parameters["path"] = "/gltf" + Model.Observables.selection[0]["elementAbsolutPath"];
        }
        return execute(operationName, parameters);
    }

    function resolvePromiseToCard(promise, successMessage = undefined, errorAsInfo = false, logSuccess = true, logError = true){
        applicationData.busyCursor()
        promise.then(function (message) {
             applicationData.resetCursor()
             if (logSuccess) {
                if (message !== "") {
                    applicationData.success(message)
                } else if (successMessage !== undefined) {
                    applicationData.success(successMessage)
                }
             }
             }).catch(function (message) {
                applicationData.resetCursor()
                if (logError) {
                    if (message === "") {
                        message = "Operation failed"
                    }
                    if (errorAsInfo) {
                        applicationData.info(message)
                    } else {
                        applicationData.error(message)
                    }
                }
            })
    }
}
