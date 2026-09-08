cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

project(OpenGL-Renderer)

set(Tools_DIR "${CMAKE_CURRENT_LIST_DIR}/Tools")
set(Runtime_DIR "${Tools_DIR}/Runtime")

include("${Runtime_DIR}/cmake/utils.cmake")
include("${Runtime_DIR}/cmake/defines.cmake")
include("${Tools_DIR}/cmake/defines.cmake")
include("defines.cmake")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT OpenGL-Renderer)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	add_definitions(-DGL_SILENCE_DEPRECATION)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Disable the runtime openGL renderer
set(UX3D_MODULE_RENDERER OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(${Tools_DIR})
add_subdirectory(renderer)
add_subdirectory(thirdparty)

install(TARGETS OpenGL-Renderer COMPONENT Runtime CONFIGURATIONS Release)
