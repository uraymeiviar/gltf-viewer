#[=======================================================================[.rst:
findcatch2
----------

Try to find the catch2 header only unit testing framework

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables::

  catch2_FOUND          - True if catch2 was found
  catch2_INCLUDE_DIRS   - include directories for catch2

The module will also define one cache variables::

  catch2_INCLUDE_DIR    - the include directory

#]=======================================================================]

if(NOT EXISTS "${catch2_INCLUDE_DIR}")
	find_path(catch2_INCLUDE_DIR
		NAMES "catch2/catch.hpp"
		DOC "catch2 library header files"
		PATHS ${UX3D_RUNTIME_DIR} ${CMAKE_SOURCE_DIR} ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
		PATH_SUFFIXES "thirdparty/catch2/single_include")
endif()

if(EXISTS "${catch2_INCLUDE_DIR}")
	set(catch2_FOUND 1)
else()
	set(catch2_FOUND 0)
endif()

set(catch2_INCLUDE_DIRS ${catch2_INCLUDE_DIR})
