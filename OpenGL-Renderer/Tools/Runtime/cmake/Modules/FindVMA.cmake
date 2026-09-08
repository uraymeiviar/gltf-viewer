#[=======================================================================[.rst:
FindVMA
----------

Try to find the VulkanMemoryAllocator header-only-library

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables::

  VMA_FOUND          - True if the library was found
  VMA_INCLUDE_DIRS   - include directories for the library

The module will also define the cache variables::

  VMA_INCLUDE_DIR    - the include directory

#]=======================================================================]

if(NOT EXISTS "${VMA_INCLUDE_DIR}")
    find_path(VMA_INCLUDE_DIR
            NAMES vk_mem_alloc.h
            DOC "VMA library header files"
            PATHS ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../ ${CMAKE_CURRENT_SOURCE_DIR}/../../
            PATH_SUFFIXES thirdparty/VulkanMemoryAllocator/src
            )
endif()

if(EXISTS "${VMA_INCLUDE_DIR}")
    set(VMA_FOUND 1)
else()
    set(VMA_FOUND 0)
endif()

set(VMA_INCLUDE_DIRS "${VMA_INCLUDE_DIR}")