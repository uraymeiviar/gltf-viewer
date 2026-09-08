#[=======================================================================[.rst:
Findgles
----------

Try to find the gles library

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables::

    gles_FOUND          - True if the library was found
    gles_INCLUDE_DIRS   - include directories for the library
    gles_LIBRARIES - link against this library to use OpenGL ES

The module will also define the cache variables::

    gles_INCLUDE_DIR    - the include directory
    gles_LIBRARY - the path to the OpenGL ES library
    egl_LIBRARY - the path to the EGL library

#]=======================================================================]

if(CMAKE_SYSTEM_NAME MATCHES "Android")
	find_library(GLESv2-lib GLESv2)
	set(gles_LIBRARY ${GLESv2-lib})
	set(gles_HEADERS "${ANDROID_NDK}/sources/android/native_app_glue/")
elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    set(gles_HEADERS "/usr/include")
    set(gles_LIBS "/usr/lib")
    set(lib_name "libGLESv2.so")
else()
    set(gles_HEADERS "${CMAKE_SOURCE_DIR}/thirdparty/Native_SDK/include")

    if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Windows")
        set(lib_name "libGLESv2.lib")
        set(gles_LIBS "${CMAKE_SOURCE_DIR}/thirdparty/Native_SDK/lib/Windows_x86_64")
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Linux")
        set(lib_name "libGLESv2.so")
        set(gles_LIBS "${CMAKE_SOURCE_DIR}/thirdparty/Native_SDK/lib/Linux_x86_64")
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
        set(lib_name "MoltenGL.framework")
        set(gles_LIBS "${CMAKE_SOURCE_DIR}/thirdparty/MoltenGL/MoltenGL/macOS/framework")
        set(gles_HEADERS "${CMAKE_SOURCE_DIR}/thirdparty/MoltenGL/MoltenGL/RedirectHeaders/include/OpenGLES")
    endif()
endif()

if(NOT EXISTS "${gles_INCLUDE_DIR}")
    set(gles_INCLUDE_DIR "${gles_HEADERS}" CACHE STRING "")
endif()

if(NOT EXISTS "${gles_LIBRARY}")
    set(gles_LIBRARY "${gles_LIBS}/${lib_name}" CACHE STRING "")
endif()

if(EXISTS "${gles_INCLUDE_DIR}" AND EXISTS "${gles_LIBRARY}")
    set(gles_FOUND 1)
else()
    set(gles_FOUND 0)

    if (NOT EXISTS "${gles_INCLUDE_DIR}")
        message(WARNING "GLES include directory not found at '${gles_INCLUDE_DIR}'")
    endif()
    if (NOT EXISTS "${gles_LIBRARY}")
        message(WARNING "GLES library not found at '${gles_LIBRARY}'")
    endif()
endif()

set(gles_INCLUDE_DIRS "${gles_INCLUDE_DIR}")
set(gles_LIBRARIES "${gles_LIBRARY}")
