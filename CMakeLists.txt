cmake_minimum_required(VERSION 3.20.2 FATAL_ERROR)

project(Glider VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Renderer_DIR "${CMAKE_CURRENT_LIST_DIR}/OpenGL-Renderer")
set(Tools_DIR "${Renderer_DIR}/Tools")
set(Runtime_DIR "${Tools_DIR}/Runtime")
set(UX3D_PROJECT_NAME "Glider" CACHE STRING "Name of our application")
set(UX3D_OUTPUT_NAME "Glider" CACHE STRING "Name of our application without spaces")
set(UX3D_VERSION "1.0.0" CACHE STRING "The version number of Glider")
set(UX3D_BUILD_NUMBER "0" CACHE STRING "Incremental build number")
set(UX3D_ENABLE_CCACHE OFF CACHE BOOL "Use ccache")
option(GLIDER_GL_DEBUG_CHECKS "Enable per-command OpenGL error checks in Release builds" OFF)

include("${Runtime_DIR}/cmake/defines.cmake")
include("${Tools_DIR}/cmake/defines.cmake")
include("${Renderer_DIR}/defines.cmake")
include("cmake/defines.cmake")
include("cmake/helpers.cmake")
include("cmake/GroupSourcesByFolder.cmake")

# Multithreading and static CRT for MSVC
if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ob1")
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(APPLE)
	set(CMAKE_SKIP_BUILD_RPATH TRUE)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
	set(CMAKE_INSTALL_RPATH "/usr/lib/swift;@loader_path/Frameworks;@executable_path/../Frameworks")
endif()

# Fetch and configure standalone third-party dependencies (GLFW, ImGui, ImGuizmo, Draco, WebP, etc.)
include(cmake/standalone_dependencies.cmake)

set(UX3D_RESOURCES_ROOT_DIR "${PROJECT_SOURCE_DIR}" CACHE INTERNAL "" FORCE)
set(UX3D_EDITOR_RESOURCES_DIR "${UX3D_RESOURCES_ROOT_DIR}/resources" CACHE INTERNAL "" FORCE)
set(UX3D_RENDERER_RESOURCES_DIR "${UX3D_RESOURCES_ROOT_DIR}/OpenGL-Renderer/resources" CACHE INTERNAL "" FORCE)

add_definitions(-DUX3D_EDITOR_RESOURCES_DIR="${UX3D_EDITOR_RESOURCES_DIR}")
add_definitions(-DUX3D_RENDERER_RESOURCES_DIR="${UX3D_RENDERER_RESOURCES_DIR}")

# Core OpenGL-Renderer
add_subdirectory("OpenGL-Renderer")

#--------------------------------------------------------------------------------
# Glider Standalone Application (Dear ImGui)
#--------------------------------------------------------------------------------
set(APP_NAME "Glider")

set(GLIDER_SOURCES
	"source/imgui/main.cpp"
	"source/imgui/Profiling.cpp"
	"header/imgui/Profiling.h"
	"source/imgui/GladGL.cpp"
	"header/imgui/GladGL.h"
	"source/imgui/Framebuffer.cpp"
	"header/imgui/Framebuffer.h"
	"source/imgui/StdFileSystem.cpp"
	"header/imgui/StdFileSystem.h"
	"source/imgui/GridRenderer.cpp"
	"header/imgui/GridRenderer.h"
	"source/imgui/MaterialInspector.cpp"
	"header/imgui/MaterialInspector.h"
	"source/imgui/ViewportSettingsPanel.cpp"
	"header/imgui/ViewportSettingsPanel.h"
	"source/imgui/StatisticsPanel.cpp"
	"header/imgui/StatisticsPanel.h"
	"source/imgui/SceneHierarchyPanel.cpp"
	"header/imgui/SceneHierarchyPanel.h"
	"source/imgui/ResourcesPanel.cpp"
	"header/imgui/ResourcesPanel.h"
	"source/imgui/ElementInspector.cpp"
	"header/imgui/ElementInspector.h"
	"source/imgui/ExtensionInspector.cpp"
	"header/imgui/ExtensionInspector.h"
	"source/imgui/AnimationPlayer.cpp"
	"header/imgui/AnimationPlayer.h"
	"source/imgui/AntiAliasing.cpp"
	"header/imgui/AntiAliasing.h"
	"source/imgui/BoundingBoxRenderer.cpp"
	"header/imgui/BoundingBoxRenderer.h"
	"source/renderViewInput/CameraActor.cpp"
	"header/renderViewInput/CameraActor.h"
	"source/imgui/Theme.cpp"
	"header/imgui/Theme.h"
	"source/imgui/EmbeddedVFS.cpp"
	"header/imgui/EmbeddedVFS.h"
	"source/imgui/EmbeddedResources.cpp"
	"header/imgui/zstd.h"
	"source/imgui/GltfValidator.cpp"
	"header/imgui/GltfValidator.h"
	"source/imgui/ValidationPanel.cpp"
	"header/imgui/ValidationPanel.h"
	"source/imgui/AppSettings.cpp"
	"header/imgui/AppSettings.h"
	"source/imgui/SearchPathsPanel.cpp"
	"header/imgui/SearchPathsPanel.h"
	"source/imgui/StatusBar.cpp"
	"header/imgui/StatusBar.h"
	"header/imgui/TextureUtils.h"
	"header/imgui/IconsMaterialDesignIcons.h"
	"header/imgui/stb_image_write.h"
	"source/imgui/SamplerTest.cpp"
	"header/imgui/SamplerTest.h"
	"source/imgui/VaoTest.cpp"
	"header/imgui/VaoTest.h"
	"source/imgui/PolygonModeTest.cpp"
	"header/imgui/PolygonModeTest.h"
	"source/imgui/CullWindingTest.cpp"
	"header/imgui/CullWindingTest.h"
	"source/imgui/DepthTest.cpp"
	"header/imgui/DepthTest.h"
	"source/imgui/BlendTest.cpp"
	"header/imgui/BlendTest.h"
	"source/imgui/SceneRevisions.cpp"
	"header/imgui/SceneRevisions.h"
	"source/imgui/ViewportOverrides.cpp"
	"header/imgui/ViewportOverrides.h"
)

find_package(Python3 COMPONENTS Interpreter QUIET)
if (Python3_Interpreter_FOUND)
	add_custom_command(
		OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/source/imgui/EmbeddedResources.cpp"
		       "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.vfs.zst"
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pack_resources.py"
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pack_resources.py"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		COMMENT "Packing runtime resources into embedded compressed archive"
	)
endif()

if (WIN32)
	list(APPEND GLIDER_SOURCES "resources/Icon_Application.rc")
endif()

add_executable(${APP_NAME} ${GLIDER_SOURCES})

# Backwards compatibility alias for build scripts specifying Gestaltor or Gestaltor-ImGui
add_custom_target(Gestaltor DEPENDS ${APP_NAME})
add_custom_target(Gestaltor-ImGui DEPENDS ${APP_NAME})

target_include_directories(${APP_NAME} PRIVATE
	"header"
	"header/imgui"
	"source"
	"${PROJECT_SOURCE_DIR}/OpenGL-Renderer/renderer/include"
	"${PROJECT_SOURCE_DIR}/OpenGL-Renderer/Tools/tools/filesystem/include"
	"${PROJECT_SOURCE_DIR}/OpenGL-Renderer/Tools/tools/gltf/include"
	"${PROJECT_SOURCE_DIR}/OpenGL-Renderer/Tools/Runtime/runtime/core/include"
	"${PROJECT_SOURCE_DIR}/OpenGL-Renderer/Tools/Runtime/runtime/math/include"
	"${PROJECT_SOURCE_DIR}/OpenGL-Renderer/Tools/Runtime/thirdparty/glm"
)

target_link_libraries(${APP_NAME} PRIVATE
	imgui
	imguizmo
	pfd
	glad
	glfw
	OpenGL-Renderer
	Gltf
	Filesystem
	ImageFormats
	Utils
	Mementos
	DataStorage
	draco::draco
	WebP::WebP
	KTX::ktx
	nlohmann_json_schema_validator
	tinyexr
)

set_target_properties(${APP_NAME} PROPERTIES
	OUTPUT_NAME "Glider"
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
	WIN32_EXECUTABLE TRUE
)

if (WIN32 AND MSVC)
	target_link_options(${APP_NAME} PRIVATE "/ENTRY:mainCRTStartup")
endif()

