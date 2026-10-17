set(UX3D_DEFINES_PRINTED FALSE CACHE INTERNAL "")

if (NOT ${UX3D_DEFINES_PRINTED})
    message(STATUS)
    message(STATUS "CMake options added by Gestaltor 'defines.cmake':")
endif()

function(cmake_option option description default)
	option(${option} description ${default})
	if (NOT ${UX3D_DEFINES_PRINTED})
		if(${option})
			message(STATUS "'${option}' is TRUE")
		else()
			message(STATUS "'${option}' is FALSE")
		endif()
	endif()
endfunction()

cmake_option(UX3D_GESTALTOR_BUNDLE_RESOURCES "Bundle resources with the executable (required for deployment)" OFF)

set(UX3D_DEFINES_PRINTED TRUE CACHE INTERNAL "")

set(CMAKE_DEBUG_POSTFIX "" CACHE INTERNAL "" FORCE)

# Set default for shared / dynamic libraries
if (APPLE)
	option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
else()
	option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (APPLE OR (GESTALTOR_BUILD_IMGUI AND NOT GESTALTOR_BUILD_QT))
	set(UX3D_MODULE_LINKAGE STATIC)
else()
	set(UX3D_MODULE_LINKAGE SHARED)
endif()

if(APPLE)
	set(MACOSX_DEPLOYMENT_TARGET "10.15")
	set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
	set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "MacOS architecture used for building")
	if(NOT (CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64"))
		message(FATAL_ERROR "CMAKE_OSX_ARCHITECTURES ${CMAKE_OSX_ARCHITECTURES} is not supported. ")
	endif()
	set(UX3D_CODESIGN_IDENTITY "Apple Development" CACHE STRING "Xcode codesign identity")
endif()