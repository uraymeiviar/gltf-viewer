#[=======================================================================[.rst:
Findjson
----------

Try to find the json header-only-library

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables::

  json_FOUND          - True if json was found
  json_INCLUDE_DIRS   - include directories for json

The module will also define one cache variables::

  json_INCLUDE_DIR    - the include directory

#]=======================================================================]

if(NOT EXISTS "${json_INCLUDE_DIR}")
    find_path(json_INCLUDE_DIR
            NAMES json.hpp
            DOC "json library header files"
            PATHS ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../ ${CMAKE_CURRENT_SOURCE_DIR}/../../
            PATH_SUFFIXES thirdparty/json/single_include/nlohmann
            )
    set(json_INCLUDE_DIR_LEGACY json_INCLUDE_DIR)
    set(json_INCLUDE_DIR "${json_INCLUDE_DIR}/..")
    message(STATUS "Found JSON ${json_INCLUDE_DIR}")
endif()

if(EXISTS "${json_INCLUDE_DIR}")
    set(json_FOUND 1)
else()
    set(json_FOUND 0)
endif()

set(json_INCLUDE_DIRS ${json_INCLUDE_DIR} ${json_INCLUDE_DIR_LEGACY})