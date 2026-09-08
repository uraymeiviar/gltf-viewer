cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

set(UX3D_RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Runtime CACHE STRING "")
add_subdirectory(${UX3D_RUNTIME_DIR})

include("${UX3D_RUNTIME_DIR}/cmake/defines.cmake")
include("cmake/defines.cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(Tools)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules/")
list(APPEND CMAKE_MODULE_PATH "${UX3D_RUNTIME_DIR}/cmake/Modules/")

add_compile_definitions(UX3D_TEST_RESOURCES="${PROJECT_SOURCE_DIR}/tests/test_resources")

add_subdirectory(thirdparty)
add_subdirectory(tools)

if (${UX3D_TOOLS_TESTING})
	add_subdirectory(tests)
	set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME}_Tests)
endif()
