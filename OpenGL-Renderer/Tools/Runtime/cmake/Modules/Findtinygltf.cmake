#[=======================================================================[.rst:
Findtinygltf
----------

Try to find the tinygltf header-only-library

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables::

  tinygltf_FOUND          - True if the library was found
  tinygltf_INCLUDE_DIRS   - include directories for the library

The module will also define the cache variables::

  tinygltf_INCLUDE_DIR    - the include directory

#]=======================================================================]

if(NOT EXISTS "${tinygltf_INCLUDE_DIR}")
    find_path(tinygltf_INCLUDE_DIR
            NAMES tiny_gltf.h
            DOC "tinygltf library header files"
            PATHS ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../ ${CMAKE_CURRENT_SOURCE_DIR}/../../
            PATH_SUFFIXES thirdparty/tinygltf
            )
endif()

if(EXISTS "${tinygltf_INCLUDE_DIR}")
    set(tinygltf_FOUND 1)
else()
    set(tinygltf_FOUND 0)
endif()

set(tinygltf_INCLUDE_DIRS "${tinygltf_INCLUDE_DIR}")