// This script can be used to make npm packages available to QML.
// Just use import / export block like the one below for a new package
// and import it in QML with
//
// import "./js/thirdparty.qml.mjs" as Thirdparty;
// 
// you can then for example use the package like this: 
//
// Thirdparty.jsonpointer.get(obj, "/path/to/json")

import * as jsonpointer from 'jsonpointer'
import * as glmatrix from 'gl-matrix'


export {glmatrix};
export {jsonpointer};