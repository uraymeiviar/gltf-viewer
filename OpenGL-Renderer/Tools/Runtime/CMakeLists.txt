cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

set(ux3d_utils "${CMAKE_CURRENT_LIST_DIR}/cmake/utils.cmake")
set(UX3D_DEFINES_PRINTED FALSE CACHE INTERNAL "")
include("${ux3d_utils}")
include("cmake/defines.cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(Runtime)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules/")
add_compile_definitions(UX3D_TEST_RESOURCES="${CMAKE_CURRENT_LIST_DIR}/tests/test_resources")

add_subdirectory(thirdparty)
add_subdirectory(runtime)

if (${UX3D_RUNTIME_TESTING})
	add_subdirectory(tests)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME}_Tests)
endif()

message(STATUS "")
