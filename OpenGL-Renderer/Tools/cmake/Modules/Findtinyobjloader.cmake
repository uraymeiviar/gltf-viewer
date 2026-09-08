#[=======================================================================[.rst:
Findtinyobjloader
----------

Try to find the tinyobjloader library

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables::

  tinyobjloader_FOUND          - True if the library was found
  tinyobjloader_INCLUDE_DIRS   - include directories for the library

The module will also define the cache variables::

  tinyobjloader_INCLUDE_DIR    - the include directory

#]=======================================================================]

if(NOT EXISTS "${tinyobjloader_INCLUDE_DIR}")
    find_path(tinyobjloader_INCLUDE_DIR
            NAMES tiny_obj_loader.h
            DOC "tinyobjloader library header files"
            PATHS ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../ ${CMAKE_CURRENT_SOURCE_DIR}/../../
            PATH_SUFFIXES thirdparty/tinyobjloader
            )
endif()

if(EXISTS "${tinyobjloader_INCLUDE_DIR}")
    set(tinyobjloader_FOUND 1)
else()
    set(tinyobjloader_FOUND 0)
endif()

set(tinyobjloader_INCLUDE_DIRS ${tinyobjloader_INCLUDE_DIR})