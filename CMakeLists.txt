cmake_minimum_required(VERSION 3.20.2 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Renderer_DIR "${CMAKE_CURRENT_LIST_DIR}/OpenGL-Renderer")
set(Tools_DIR "${Renderer_DIR}/Tools")
set(Runtime_DIR "${Tools_DIR}/Runtime")
set(UX3D_PROJECT_NAME "Gestaltor" CACHE STRING "Name of our application")
set(UX3D_OUTPUT_NAME "Gestaltor" CACHE STRING "Name of our application without spaces")
set(UX3D_VERSION "2.0.0" CACHE STRING "The version number of Gestaltor. Must consist of integers separated by dots (cmake requirement)")
set(UX3D_BUILD_NUMBER "0" CACHE STRING "Incremental build number")
set(UX3D_ENABLE_CCACHE OFF CACHE BOOL "Use ccache")

message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
include("${Runtime_DIR}/cmake/defines.cmake")
include("${Tools_DIR}/cmake/defines.cmake")
include("${Renderer_DIR}/defines.cmake")
include("cmake/defines.cmake")
include(cmake/helpers.cmake)
include(cmake/gather_frameworks.cmake)
include(cmake/GroupSourcesByFolder.cmake)

option(GESTALTOR_BUILD_IMGUI "Build standalone Dear ImGui application" ON)
option(GESTALTOR_BUILD_QT "Build legacy Qt 6 GUI application" OFF)

if (GESTALTOR_BUILD_QT)
	include(cmake/conan.cmake)
endif()


#Add multithreading to msvc
if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	# Disable Runtime checks to increase debug performance
	string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	# Enable inlining level 1 to increase debug performance
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ob1")
	if (GESTALTOR_BUILD_IMGUI AND NOT GESTALTOR_BUILD_QT)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
endif()

# Ensure only one CMake Project is generated
set(CMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY True)

#--------------------------------------------------------------------------------
# RPATH settings

if(APPLE)
	# Ensure the rpath is already deployable for build targets (i.e. doesn't
	# contain absolute paths into the build tree)
    set(CMAKE_SKIP_BUILD_RPATH TRUE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
	set(CMAKE_INSTALL_RPATH "/usr/lib/swift;@loader_path/Frameworks;@executable_path/../Frameworks")
endif()

if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")

	# use, i.e. don't skip the full RPATH for the build tree
	set(CMAKE_SKIP_BUILD_RPATH FALSE)

	# when building, don't use the install RPATH already
	# (but later on when installing)
   #	set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)#org
 	set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

	# the RPATH to be used when installing
	set(CMAKE_INSTALL_RPATH "\$ORIGIN")

	# add the automatically determined parts of the RPATH
	# which point to directories outside the build tree to the install RPATH
  	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE) 

	set(CMAKE_SKIP_RPATH FALSE)
endif()

#--------------------------------------------------------------------------------
# Create project

if(APPLE)
	# set xcode to use dsym (helpful for Instruments profiling)
	# set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
endif()

# project name
project("Gestaltor" LANGUAGES C CXX)
set(PROJECT_VERSION ${UX3D_VERSION})

if (NOT GESTALTOR_BUILD_QT)
	include(cmake/standalone_dependencies.cmake)
endif()


if(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
	add_compile_options("-fcolor-diagnostics")
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES GNU)
	add_compile_options("-fdiagnostics-color=always")
endif()

if(UX3D_ENABLE_CCACHE)
 	set(CMAKE_C_COMPILER_LAUNCHER "ccache")
	set(CMAKE_CXX_COMPILER_LAUNCHER "ccache")
endif()

add_subdirectory(subprocess)

if (GESTALTOR_BUILD_QT)
#--------------------------------------------------------------------------------
# run and configure conan
# https://github.com/conan-io/cmake-conan

if (APPLE)
	message(STATUS "CMAKE_OSX_ARCHITECTURES ${CMAKE_OSX_ARCHITECTURES}")
	if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
		conan_cmake_autodetect(settings BUILD_TYPE "Release" ARCH "armv8")
		set(conan_build_profile ${CMAKE_CURRENT_LIST_DIR}/conan/profiles/apple_silicon)
	elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
		conan_cmake_autodetect(settings BUILD_TYPE "Release" ARCH "x86_64")
		set(conan_build_profile ${CMAKE_CURRENT_LIST_DIR}/conan/profiles/apple_x86_64)
	else()
		message(FATAL_ERROR "Unsupported CMAKE_OSX_ARCHITECTURES ${CMAKE_OSX_ARCHITECTURES}")
	endif()
	list(APPEND settings os.version=10.15)
	if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
		set(conan_host_profile ${CMAKE_CURRENT_LIST_DIR}/conan/profiles/apple_silicon)
	else()
		set(conan_host_profile ${CMAKE_CURRENT_LIST_DIR}/conan/profiles/apple_x86_64)
	endif()
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
	conan_cmake_autodetect(settings BUILD_TYPE "Release")
else()
	if (${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
		conan_cmake_autodetect(settings BUILD_TYPE "Release")
	else()
		conan_cmake_autodetect(settings BUILD_TYPE "${CMAKE_BUILD_TYPE}")
	endif()
endif()

list(FILTER settings EXCLUDE REGEX "compiler.cppstd=*")

if (WIN32 AND MSVC AND MSVC_VERSION GREATER_EQUAL 1930)
	# Pin Conan dependency resolution to the VS2022 toolchain family used in CI.
	list(FILTER settings EXCLUDE REGEX "^compiler.version=")
	list(FILTER settings EXCLUDE REGEX "^compiler.toolset=")
	list(APPEND settings compiler.version=17 compiler.toolset=v143)
endif()

set(conan_extra_build_args)
if (WIN32 AND "$ENV{GESTALTOR_CONAN_BUILD_KTX}" STREQUAL "1")
	# Force KTX build from source to avoid mixing binaries built with newer MSVC updates.
	message(STATUS "Conan: forcing source build for ktx/* because GESTALTOR_CONAN_BUILD_KTX=1")
	list(APPEND conan_extra_build_args BUILD ktx/*)
endif()

set(ENV{CONAN_CMAKE_GENERATOR} "Ninja")
set(ENV{CONAN_REVISIONS_ENABLED} "1")

conan_add_remote(NAME ux3d-conan-repo
                 URL https://ux3d.jfrog.io/artifactory/api/conan/default-conan
                 VERIFY_SSL True)


# Install openssl first
list(APPEND conan_cmake_configure_options_openssl openssl:no_zlib=True)

conan_cmake_configure(
	REQUIRES 
		openssl/3.0.8
    GENERATORS 
		cmake_find_package
		cmake_paths
	OPTIONS
		${conan_cmake_configure_options_openssl}
	)

if (APPLE)
	conan_cmake_install(PATH_OR_REFERENCE .
					BUILD missing
					${conan_extra_build_args}
					REMOTE ux3d-conan-repo
					PROFILE_HOST ${conan_host_profile}
					PROFILE_BUILD ${conan_build_profile}
					SETTINGS ${settings}
					)
else()
	conan_cmake_install(PATH_OR_REFERENCE .
					BUILD missing
					${conan_extra_build_args}
					REMOTE ux3d-conan-repo
					SETTINGS ${settings}
					)
endif()

include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
set(OPENSSL_PATH ${CONAN_OPENSSL_ROOT})

set(QTBASE_CMAKE_ARGS "-DQT_FEATURE_openssl_linked=ON -DOPENSSL_ROOT_DIR=\"${OPENSSL_PATH}\" -DOPENSSL_USE_STATIC_LIBS=ON")
if (WIN32)
	list(APPEND conan_cmake_configure_options qtbase:openssl=yes)
endif()

if (MSVC)
	if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		list(APPEND conan_cmake_configure_options qtbase:debug=True)
		list(APPEND conan_cmake_configure_options qtbase:release=False)
	else()
		list(APPEND conan_cmake_configure_options qtbase:debug=False)
		list(APPEND conan_cmake_configure_options qtbase:release=True)
	endif()
endif()

if (APPLE)
	# Disable crosscompile for apple: https://cmake.org/cmake/help/latest/variable/CMAKE_CROSSCOMPILING.html
	set(qt_apple_cmake_options "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES} -DCMAKE_INSTALL_RPATH=@loader_path/../../.. -DCMAKE_CROSSCOMPILING=OFF")

	set(QTBASE_CMAKE_ARGS "${QTBASE_CMAKE_ARGS} ${qt_apple_cmake_options}")
	list(APPEND conan_cmake_configure_options qt5compat:cmake_args_leaf_module=${qt_apple_cmake_options})
	list(APPEND conan_cmake_configure_options qtdeclarative:cmake_args_leaf_module=${qt_apple_cmake_options})
	list(APPEND conan_cmake_configure_options qtsvg:cmake_args_leaf_module=${qt_apple_cmake_options})
	list(APPEND conan_cmake_configure_options qttools:cmake_args_leaf_module=${qt_apple_cmake_options})
	list(APPEND conan_cmake_configure_options qtstatemachine:cmake_args_leaf_module=${qt_apple_cmake_options})
	list(APPEND conan_cmake_configure_options qtstatemachineqml:cmake_args_leaf_module=${qt_apple_cmake_options})
	list(APPEND conan_cmake_configure_options qtlanguageserver:cmake_args_leaf_module=${qt_apple_cmake_options})
endif()

list(APPEND conan_cmake_configure_options qtbase:zlib=qt qtbase:libjpeg=qt qtbase:libpng=qt qtbase:freetype=None qtbase:pcre=None qtbase:harfbuzz=None)

if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
	# For xcb: sudo apt-get install libxcb-* libxkbcommon-x11-dev pkg-config
	list(APPEND conan_cmake_configure_options qtbase:xcb=True qtbase:xkbcommon=True)
	set(QTBASE_CMAKE_ARGS "${QTBASE_CMAKE_ARGS} -DQT_FEATURE_xkbcommon_x11=ON")
endif()

# Add static openssl to qt
list(APPEND conan_cmake_configure_options qtbase:openssl_linked=True qtbase:cmake_args_qtbase="${QTBASE_CMAKE_ARGS}")
list(APPEND conan_dependencies
		qt-conan-common/6.3@qt/everywhere
		qtbase/6.3.1@qt/everywhere
		qtdeclarative/6.3.1@qt/everywhere
		qtsvg/6.3.1@qt/everywhere
		qttools/6.3.1@qt/everywhere
		qtstatemachine/6.3.1@qt/everywhere
		qtstatemachineqml/6.3.1@qt/everywhere
		qt5compat/6.3.1@qt/everywhere
		qtlanguageserver/6.3.1@qt/everywhere
		qtshadertools/6.3.1@qt/everywhere
		qtimageformats/6.3.1@qt/everywhere
		qtactiveqt/6.3.1@qt/everywhere
		assimp/5.2.2
		draco/1.5.2
		nlohmann_json/3.11.3
		libwebp/1.4.0
		json-schema-validator/2.3.0
		ktx/4.0.0
		openal/1.22.0
		minimp3/20200304
		stb/cci.20210910
		fmt/9.1.0
)

if (WIN32)
	list(APPEND conan_dependencies qtinstallerframework/4.3)
endif()

conan_cmake_configure(
	REQUIRES 
		# See https://wiki.qt.io/Using_Conan_for_Qt6#Further_notes
		${conan_dependencies}
    GENERATORS 
		cmake_find_package
		cmake_paths
	OPTIONS
		# Ensure Qt is cross built for intel on macOS as Qt ignores the setting
		# in the conan profile
		${conan_cmake_configure_options}
	)

if (APPLE)
	conan_cmake_install(PATH_OR_REFERENCE .
						BUILD missing
						${conan_extra_build_args}
						REMOTE ux3d-conan-repo
						PROFILE_HOST ${conan_host_profile}
						PROFILE_BUILD ${conan_build_profile}
						SETTINGS ${settings}
						)
else()
	conan_cmake_install(PATH_OR_REFERENCE .
						BUILD missing
						${conan_extra_build_args}
						REMOTE ux3d-conan-repo
						SETTINGS ${settings}
						)
endif()

#--------------------------------------------------------------------------------
# Configure dependencies

#include CPM and install cmake packages
include(cmake/GroupSourcesByFolder.cmake)

# Include paths to conan packages and extend cmake modules so
# that conan packages can be used as regular cmake targets
include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
# Extract paths from conan package paths so that the conan 
# variables don't scatter throughout this CMakeLists.txt
set(QTBASE_PATH ${CONAN_QTBASE_ROOT})
set(QTDECLARATIVE_PATH ${CONAN_QTDECLARATIVE_ROOT})
set(QTSVG_PATH ${CONAN_QTSVG_ROOT})
set(QTTOOLS_PATH ${CONAN_QTTOOLS_ROOT})
set(QT5COMPAT_PATH ${CONAN_QT5COMPAT_ROOT})
set(QTSTATEMACHINE_PATH ${CONAN_QTSTATEMACHINE_ROOT})
set(QTSTATEMACHINEQML_PATH ${CONAN_QTSTATEMACHINEQML_ROOT})

if (WIN32)
	set(QTINSTALLERFRAMEWORK_PATH ${CONAN_QTINSTALLERFRAMEWORK_ROOT})
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
list(APPEND CMAKE_MODULE_PATH "${Runtime_DIR}/cmake/Modules")

# run MOC precompiler automatically when needed
set(CMAKE_AUTOMOC ON)

# create code from list of Qt designer ui files
set(CMAKE_AUTOUIC ON)

# define paths in the project that hold ui files
set(CMAKE_AUTOUIC_SEARCH_PATHS "ui" "ui/wire_frames" "ui/properties" "ui/title_docks")

# cmake will call the rcc compiler the qrc resource files
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED) 
find_package(Qt6Quick REQUIRED) 
find_package(Qt6Qml REQUIRED) 
find_package(Qt6LabsQmlModels REQUIRED) 
find_package(Qt6QmlWorkerScript REQUIRED) 
find_package(Qt6QuickWidgets REQUIRED) 
find_package(Qt6QuickControls2 REQUIRED)
find_package(Qt6QuickControls2Impl REQUIRED)
find_package(Qt6QuickLayouts REQUIRED)
find_package(Qt6QuickDialogs2 REQUIRED)
find_package(Qt6QuickDialogs2Utils REQUIRED)
find_package(Qt6QuickDialogs2QuickImpl REQUIRED)
find_package(Qt6QuickTemplates2 REQUIRED)
find_package(Qt6QuickShapesPrivate REQUIRED)
find_package(Qt6Widgets REQUIRED) 
find_package(Qt6Network REQUIRED) 
find_package(Qt6Core5Compat REQUIRED)
find_package(Qt6OpenGLWidgets REQUIRED)
find_package(Qt6PrintSupport REQUIRED) 
find_package(Qt6Svg REQUIRED) 
find_package(Qt6DBus REQUIRED)
find_package(Qt6OpenGL REQUIRED)
find_package(Qt6StateMachine REQUIRED)
find_package(Qt6StateMachineQml REQUIRED)
find_package(assimp REQUIRED)

# The plugins that should be deployed to the application bundle must be 
# explicitly specified here. Usually the windeployqt / macdeployqt tool
# would take care that these plugins are contained in the bundle at the
# correct location, for mac app store builds however macdeployqt cannot
# be used.
set(qt_plugins 
	"${QTSVG_PATH}/plugins/iconengines"
	"${QTSVG_PATH}/plugins/imageformats"
	"${QTBASE_PATH}/plugins/imageformats"
	"${QTBASE_PATH}/plugins/platforms"
	"${QTBASE_PATH}/plugins/styles"
	"${QTBASE_PATH}/plugins/tls"
)
endif() # GESTALTOR_BUILD_QT

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/usd_from_gltf.cmake")
	include(thirdparty/usd_from_gltf.cmake)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/gltf-validator.cmake")
	include(thirdparty/gltf-validator.cmake)
endif()

set(UX3D_RESOURCES_ROOT_DIR "${PROJECT_SOURCE_DIR}" CACHE INTERNAL "" FORCE)
set(UX3D_EDITOR_RESOURCES_DIR "${UX3D_RESOURCES_ROOT_DIR}/resources" CACHE INTERNAL "" FORCE)
set(UX3D_RENDERER_RESOURCES_DIR "${UX3D_RESOURCES_ROOT_DIR}/OpenGL-Renderer/resources" CACHE INTERNAL "" FORCE)

add_definitions(-DUX3D_EDITOR_RESOURCES_DIR="${UX3D_EDITOR_RESOURCES_DIR}")
add_definitions(-DUX3D_RENDERER_RESOURCES_DIR="${UX3D_RENDERER_RESOURCES_DIR}")

add_subdirectory("OpenGL-Renderer")
add_subdirectory("editor")
add_subdirectory("thirdparty")
if (GESTALTOR_BUILD_QT)
	add_subdirectory("automated")
endif()

if (GESTALTOR_BUILD_QT)
# add all relevant project files to cmake variables
add_sources("source/*.cpp" "sources")
if (APPLE)
	add_sources("source/*.mm" "sources")
	add_sources("source/helpers/*.mm" "sources")
endif()
add_sources("source/*.swift" "sources")
add_sources("source/converters/*.cpp" "sources")
add_sources("source/interactors/*.cpp" "sources")
add_sources("source/widgets/*.cpp" "sources")
add_sources("source/titleWidgets/*.cpp" "sources")
add_sources("source/promoters/*.cpp" "sources")
add_sources("source/helpers/*.cpp" "sources")
add_sources("source/operations/*.cpp" "sources")
add_sources("source/widgets/entries/*.cpp" "sources")
add_sources("source/widgets/popups/*.cpp" "sources")
add_sources("source/widgets/content/*.cpp" "sources")
add_sources("source/widgets/renderContext/*.cpp" "sources")
add_sources("source/enums/*.cpp" "sources")
add_sources("source/renderViewInput/*.cpp" "sources")
add_sources("source/thirdparty/*.cpp" "sources")

add_sources("header/*.h" "headers")
add_sources("header/converters/*.h" "headers")
add_sources("header/interactors/*.h" "headers")
add_sources("header/useCases/*.h" "headers")
add_sources("header/interfaces/*.h" "headers")
add_sources("header/operations/*.h" "headers")
add_sources("header/widgets/*.h" "headers")
add_sources("header/widgets/entries/*.h" "headers")
add_sources("header/widgets/popups/*.h" "headers")
add_sources("header/widgets/content/*.h" "headers")
add_sources("header/widgets/renderContext/*.h" "headers")
add_sources("header/titleWidgets/*.h" "headers")
add_sources("header/promoters/*.h" "headers")
add_sources("header/helpers/*.h" "headers")
add_sources("header/enums/*.h" "headers")
add_sources("header/renderViewInput/*.h" "headers")
add_sources("header/thirdparty/*.h" "headers")

add_sources("resources/style_sheets/*.qrc" "qt_resources")
add_sources("editor/qml/QML.qrc" "qt_resources")
add_sources("BuildId.qrc" "qt_resources")
add_sources("Fonts.qrc" "qt_big_resources")
add_sources("Images.qrc" "qt_big_resources")
add_sources("Models.qrc" "qt_big_resources")
add_sources("SampleGltf.qrc" "qt_big_resources")
# Shaders need to be added as "source" style resource as 
# qt will not detect changes when editing shaders otherwise
add_sources("Shaders.qrc" "qt_resources")
add_sources("resources/environments/*.qrc" "qt_big_resources")

if(${CMAKE_GENERATOR} MATCHES "Xcode")	
	list(APPEND xcode_copy_resources "${PROJECT_SOURCE_DIR}/resources/qt.conf")
	list(APPEND xcode_copy_resources "${PROJECT_SOURCE_DIR}/resources/Assets.xcassets")

	# Mac Application bundle plugins need to be added to Xcode as sources
	gather_xcode_copy_files(xcode_copy_qml_plugin PLUGIN_DIRECTORIES "${QTDECLARATIVE_PATH}/qml" "${QTSTATEMACHINEQML_PATH}/qml")
	gather_xcode_copy_files(xcode_copy_plugins PLUGIN_DIRECTORIES ${qt_plugins})
	gather_xcode_copy_files(xcode_copy_usd PLUGIN_DIRECTORIES "${usd_install_dir}/lib")
	list(APPEND xcode_copy_executables "${ufg_install_dir}/bin/usd_from_gltf")
	list(APPEND xcode_copy_executables "${gltf-validator_SOURCE_DIR}/build/bin/gltf_validator")
endif()

set(app_icon "resources/Icon_Application.rc")

# Convert and configure the app icon
if(APPLE)
	set(MACOSX_BUNDLE_ICON_FILE GestaltorIcon.icns)
	set(app_icon ${CMAKE_CURRENT_BINARY_DIR}/GestaltorIcon.icns)
	set(glb_icon ${CMAKE_CURRENT_BINARY_DIR}/GlbIcon.icns)
	set(gltf_icon ${CMAKE_CURRENT_BINARY_DIR}/GltfIcon.icns)
	set(gestalt_icon ${CMAKE_CURRENT_BINARY_DIR}/GestaltIcon.icns)
	execute_process(COMMAND pip3 install --user Pillow argparse RESULT_VARIABLE dump OUTPUT_VARIABLE dump ERROR_VARIABLE dump)
	execute_process(COMMAND python3 ${PROJECT_SOURCE_DIR}/cmake/convert_icon.py "${PROJECT_SOURCE_DIR}/resources/Gestaltor_AppIcon_1024.png" "${app_icon}")
	set_source_files_properties(${app_icon} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	execute_process(COMMAND python3 ${PROJECT_SOURCE_DIR}/cmake/convert_icon.py "${PROJECT_SOURCE_DIR}/resources/Gestaltor_FileIcon_glb_1024.png" "${glb_icon}")
	set_source_files_properties(${glb_icon} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	execute_process(COMMAND python3 ${PROJECT_SOURCE_DIR}/cmake/convert_icon.py "${PROJECT_SOURCE_DIR}/resources/Gestaltor_FileIcon_glTF_1024.png" "${gltf_icon}")
	set_source_files_properties(${gltf_icon} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	execute_process(COMMAND python3 ${PROJECT_SOURCE_DIR}/cmake/convert_icon.py "${PROJECT_SOURCE_DIR}/resources/Gestaltor_FileIcon_gestalt_1024.png" "${gestalt_icon}")
	set_source_files_properties(${gestalt_icon} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

# cmake creates executable using relevant project files
if(WIN32)
	qt6_add_resources(qt_sources ${qt_resources})
	qt6_add_big_resources(qt_sources ${qt_big_resources})
	add_executable(${PROJECT_NAME} WIN32 "${sources};${headers};${qt_sources}" ${app_icon} ${glb_icon} ${gltf_icon} ${gestalt_icon})
else()
	qt6_add_resources(qt_sources ${qt_resources})
	qt6_add_resources(qt_sources ${qt_big_resources})
	add_executable(${PROJECT_NAME} "${sources};${headers};${qt_sources}" ${app_icon} ${glb_icon} ${gltf_icon} ${gestalt_icon} ${xcode_copy_resources} ${xcode_copy_qml_plugin} ${xcode_copy_plugins} ${xcode_copy_usd} ${xcode_copy_executables})
endif()

if(APPLE)
	set_source_files_properties(${xcode_copy_resources} PROPERTIES
      MACOSX_PACKAGE_LOCATION Resources
	)

	# Configure plugins to be placed in the appropriate location in the 
	# application bundle. E.g., qml plugins are expected to be found in 
	# Gestaltor.app/Contents/Resources/qml by Qt
	configure_xcode_copy_files(
		CODESIGN_ON_COPY ON 
		BUNDLE_DESTINATION_DIRECTORY "Resources" 
		PLUGIN_DIRECTORIES "${QTDECLARATIVE_PATH}/qml" "${QTSTATEMACHINEQML_PATH}/qml"
	)
	configure_xcode_copy_files(
		CODESIGN_ON_COPY ON
		BUNDLE_DESTINATION_DIRECTORY "PlugIns"
		PLUGIN_DIRECTORIES ${qt_plugins}
	)

	configure_xcode_copy_files(
		CODESIGN_ON_COPY ON
		BUNDLE_DESTINATION_DIRECTORY "Resources/usd"
		PLUGIN_DIRECTORIES "${usd_install_dir}/lib/"
	)
	
	# Add the usd_from_gltf command line tool
	# https://developer.apple.com/documentation/xcode/embedding-a-helper-tool-in-a-sandboxed-app
	# see usd_from_gltf.cmake for codesigning, entitlements and rpath handling
	configure_xcode_copy_files(
		CODESIGN_ON_COPY ON
		BUNDLE_DESTINATION_DIRECTORY "MacOS"
		PLUGIN_FILES "${ufg_install_dir}/bin/usd_from_gltf" "${gltf-validator_SOURCE_DIR}/build/bin/gltf_validator"
	)

	set(dart2native_entitlements ENTITLEMENTS "${PROJECT_SOURCE_DIR}/resources/entitlements/dart2native.entitlements")

	xcode_patch_rpath(
		TARGET ${PROJECT_NAME}
		APPNAME ${UX3D_OUTPUT_NAME}
		ADD_RPATH @executable_path/../Resources/usd 
		FILE MacOS/usd_from_gltf
	)
	xcode_codesign_file(
		TARGET ${PROJECT_NAME} 
		APPNAME ${UX3D_OUTPUT_NAME}
		${subprocess_entitlements} 
		HARDENED_RUNTIME 
		CODESIGN_FILES "MacOS/usd_from_gltf"
	)
	xcode_codesign_file(
		TARGET ${PROJECT_NAME} 
		APPNAME ${UX3D_OUTPUT_NAME}
		${dart2native_entitlements} 
		HARDENED_RUNTIME
		CODESIGN_FILES "MacOS/gltf_validator"
	)
	set_source_files_properties(${ufg_install_dir}/bin/usd_from_gltf PROPERTIES XCODE_LAST_KNOWN_FILE_TYPE "compiled.mach-o.executable" XCODE_EXPLICIT_FILE_TYPE "compiled.mach-o.executable")
	set_source_files_properties(${gltf-validator_SOURCE_DIR}/build/bin/gltf_validator PROPERTIES XCODE_LAST_KNOWN_FILE_TYPE "compiled.mach-o.executable" XCODE_EXPLICIT_FILE_TYPE "compiled.mach-o.executable")
endif()

if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
	set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")
endif()

# Set output name of Gestaltor
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${UX3D_OUTPUT_NAME})

# fix the directory structure for xcode
GroupSourcesByFolder(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} Qt6::Widgets)
target_link_libraries(${PROJECT_NAME} Qt6::Network)
target_link_libraries(${PROJECT_NAME} Qt6::Quick)
target_link_libraries(${PROJECT_NAME} Qt6::QuickWidgets)
target_link_libraries(${PROJECT_NAME} Qt6::QuickControls2)
target_link_libraries(${PROJECT_NAME} Qt6::QuickControls2Impl)
target_link_libraries(${PROJECT_NAME} Qt6::QuickLayouts)
target_link_libraries(${PROJECT_NAME} Qt6::QuickDialogs2)
target_link_libraries(${PROJECT_NAME} Qt6::QuickDialogs2Utils)
target_link_libraries(${PROJECT_NAME} Qt6::QuickDialogs2QuickImpl)
target_link_libraries(${PROJECT_NAME} Qt6::OpenGLWidgets)
target_link_libraries(${PROJECT_NAME} Qt6::QmlWorkerScript)
target_link_libraries(${PROJECT_NAME} Qt6::PrintSupport)
target_link_libraries(${PROJECT_NAME} Qt6::Core5Compat)
target_link_libraries(${PROJECT_NAME} Qt6::Svg)
target_link_libraries(${PROJECT_NAME} Qt6::DBus)
target_link_libraries(${PROJECT_NAME} Qt6::Core)
target_link_libraries(${PROJECT_NAME} Qt6::Qml)
target_link_libraries(${PROJECT_NAME} Qt6::QmlModels)
target_link_libraries(${PROJECT_NAME} Qt6::LabsQmlModels)
target_link_libraries(${PROJECT_NAME} Qt6::Gui)
target_link_libraries(${PROJECT_NAME} Qt6::OpenGL)
target_link_libraries(${PROJECT_NAME} Qt6::QuickTemplates2)
target_link_libraries(${PROJECT_NAME} Qt6::QuickShapesPrivate)
target_link_libraries(${PROJECT_NAME} Qt6::StateMachine)
target_link_libraries(${PROJECT_NAME} Qt6::StateMachineQml)
target_link_libraries(${PROJECT_NAME} OpenGL-Renderer)
target_link_libraries(${PROJECT_NAME} model)
target_link_libraries(${PROJECT_NAME} Logging)
target_link_libraries(${PROJECT_NAME} Gltf)
target_link_libraries(${PROJECT_NAME} Utils)
target_link_libraries(${PROJECT_NAME} DataStorage)
target_link_libraries(${PROJECT_NAME} ImageFormats)
target_link_libraries(${PROJECT_NAME} Filesystem)
target_link_libraries(${PROJECT_NAME} Image)
target_link_libraries(${PROJECT_NAME} Animation)
target_link_libraries(${PROJECT_NAME} Core)
target_link_libraries(${PROJECT_NAME} Math)
target_link_libraries(${PROJECT_NAME} assimp::assimp)
target_link_libraries(${PROJECT_NAME} P2G2)


# OpenSSL
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})

# fmt
find_package(fmt REQUIRED)
target_link_libraries(${PROJECT_NAME} fmt::fmt)


if(APPLE)
	target_link_libraries(${PROJECT_NAME} "-framework ApplicationServices -framework Foundation")
endif()

# include own headers
target_include_directories(${PROJECT_NAME} PRIVATE "header")

if(UX3D_GESTALTOR_BUNDLE_RESOURCES)
    set(UX3D_RESOURCES_ROOT_DIR ":"  CACHE INTERNAL "" FORCE)
    set(UX3D_QML_ROOT_DIR ":")
	set(UX3D_QML_ROOT_DIR_URL "qrc:")
else()
    set(UX3D_RESOURCES_ROOT_DIR "${PROJECT_SOURCE_DIR}"  CACHE INTERNAL "" FORCE)
	set(UX3D_QML_ROOT_DIR "${PROJECT_SOURCE_DIR}/editor/qml")
	set(UX3D_QML_ROOT_DIR_URL "file:${PROJECT_SOURCE_DIR}/editor/qml")
endif()

set(UX3D_EDITOR_RESOURCES_DIR "${UX3D_RESOURCES_ROOT_DIR}/resources"  CACHE INTERNAL "" FORCE)
set(UX3D_RENDERER_RESOURCES_DIR "${UX3D_RESOURCES_ROOT_DIR}/OpenGL-Renderer/resources"  CACHE INTERNAL "" FORCE)


# set directory of shaders
add_definitions(-DUX3D_EDITOR_RESOURCES_DIR="${UX3D_EDITOR_RESOURCES_DIR}")
add_definitions(-DUX3D_RENDERER_RESOURCES_DIR="${UX3D_RENDERER_RESOURCES_DIR}")
add_definitions(-DUX3D_QML_ROOT_DIR_URL="${UX3D_QML_ROOT_DIR_URL}")
add_definitions(-DUX3D_QML_ROOT_DIR="${UX3D_QML_ROOT_DIR}")

# parse build id

if (UX3D_CI_BUILD)
	set(build_id_prefix "CI")
else()
	set(build_id_prefix "dev")
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE -DUX3D_PROJECT_NAME="${UX3D_PROJECT_NAME}")
add_definitions(-DUX3D_VERSION="${UX3D_VERSION}")

file(TOUCH "${PROJECT_SOURCE_DIR}/resources/build_id")
add_custom_target(BuildId
	WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	COMMAND "./cmake/parse_build_id.sh" "${build_id_prefix}" "${PROJECT_SOURCE_DIR}/resources/build_id")

add_dependencies(${PROJECT_NAME} BuildId)

if(APPLE)
	# Configure the bundle. As CMake's support for embedding frameworks and Xcode archive builds
	# is a relatively new addition, many things have to be specified manually. This is unfortunate
	# and maybe we can find another way to do this more elegantly in the future

    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE ON MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/resources/Info.plist.in)

	# Enable install step in xcode
	set_target_properties(${PROJECT_NAME} PROPERTIES
      XCODE_ATTRIBUTE_SKIP_INSTALL  NO
      XCODE_ATTRIBUTE_INSTALL_PATH  "$(LOCAL_APPS_DIR)"
  	)

	# Add all libraries / frameworks used by the Application to be embedded in the application
	gather_frameworks(embedded_frameworks TARGETS ${PROJECT_NAME})
	# Skip install for all dependencies to ensure the archive does not contain 
	# any targets except the Gestaltor application. If we don't do this the archive
	# would be a "generic" archive not suitable for upload to the mac app store
	xcode_dependencies_skip_install(TARGET ${PROJECT_NAME})

	set_target_properties(${PROJECT_NAME} PROPERTIES
	  	XCODE_EMBED_FRAMEWORKS "${embedded_frameworks}"
      	XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY      TRUE
    	XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY TRUE
	)
endif()

# Xcode builds allow us to set some defaults for codesigning and for the app store 
if(${CMAKE_GENERATOR} MATCHES "Xcode")
	message(STATUS "Configuring Xcode")
	set(XCODE_GENERATE_SCHEME On)
	set(XCODE_SCHEME_DEBUG_DOCUMENT_VERSIONING Off)

	SET_XCODE_PROPERTY(${PROJECT_NAME} ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")

	if (${UX3D_GESTALTOR_BUNDLE_RESOURCES})
		# If resources are bundled we can enable the Sandbox, as then all files gestaltor has to access are within the 
		# sandbox
		set(xcode_entitlements  "${PROJECT_SOURCE_DIR}/resources/Gestaltor.default.entitlements")
	else()
		# If resources should be accessed from their location on the filesystem the Sandbox has to be disabled
		# or otherwise it would forbid Gestaltor to access the resources
		set(xcode_entitlements "${PROJECT_SOURCE_DIR}/resources/Gestaltor.development.entitlements")
	endif()

	SET_XCODE_PROPERTY(${PROJECT_NAME} CODE_SIGN_ENTITLEMENTS "${xcode_entitlements}")
	SET_XCODE_PROPERTY(P2G2 CODE_SIGN_ENTITLEMENTS "${xcode_entitlements}")

	SET_XCODE_PROPERTY(${PROJECT_NAME} ENABLE_HARDENED_RUNTIME "YES")

	SET_XCODE_PROPERTY(${PROJECT_NAME} ALWAYS_EMBED_SWIFT_STANDARD_LIBRARIES "YES")

	SET_XCODE_PROPERTY(${PROJECT_NAME} PRODUCT_BUNDLE_IDENTIFIER "io.ux3d.gestaltor")

	# Set Xcode codesigning
	# https://stackoverflow.com/questions/40664125/cmake-and-code-signing-in-xcode-8-for-ios-projects
	SET_XCODE_PROPERTY(${PROJECT_NAME} CODE_SIGN_IDENTITY ${UX3D_CODESIGN_IDENTITY})
endif()

if(APPLE)
	# Configure the bundle. As CMake's support for embedding frameworks and Xcode archive builds
	# is a relatively new addition, many things have to be specified manually. This is unfortunate
	# and maybe we can find another way to do this more elegantly in the future

    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE ON MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/resources/Info.plist.in)

	# Enable install step in xcode
	set_target_properties(${PROJECT_NAME} PROPERTIES
      XCODE_ATTRIBUTE_SKIP_INSTALL  NO
      XCODE_ATTRIBUTE_INSTALL_PATH  "$(LOCAL_APPS_DIR)"
  	)

	# Add all libraries / frameworks used by the Application to be embedded in the application
	gather_frameworks(embedded_frameworks TARGETS ${PROJECT_NAME})

	# Skip install for all dependencies to ensure the archive does not contain 
	# any targets except the Gestaltor application. If we don't do this the archive
	# would be a "generic" archive not suitable for upload to the mac app store
	xcode_dependencies_skip_install(TARGET ${PROJECT_NAME})

	set_target_properties(${PROJECT_NAME} PROPERTIES
	  	XCODE_EMBED_FRAMEWORKS "${embedded_frameworks}"
      	XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY      TRUE
    	XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY TRUE
	)
endif()

# generate the config.xml used by the installer
set(UX3D_INSTALLATION_DIR "@ApplicationsDirX64@/UX3D/${UX3D_OUTPUT_NAME}/${UX3D_VERSION}")
configure_file("${PROJECT_SOURCE_DIR}/resources/config.xml.in" "${PROJECT_SOURCE_DIR}/Deployment/config/config.xml")

#--------------------------------------------------------------------------------
# Installation

SET(plugin_dest_dir bin)
SET(qtconf_dest_dir bin)
SET(APPS "${CMAKE_INSTALL_PREFIX}/bin/${UX3D_OUTPUT_NAME}")
IF(APPLE)
	SET(plugin_dest_dir "${UX3D_OUTPUT_NAME}.app/Contents/MacOS")
	SET(qtconf_dest_dir "${UX3D_OUTPUT_NAME}.app/Contents/Resources")
  SET(frameworks_dest_dir "${UX3D_OUTPUT_NAME}.app/Contents/Frameworks")
  SET(resources_dest_dir "${UX3D_OUTPUT_NAME}.app/Contents/Resources")
	SET(qt_platforms_plugin_dir "${CMAKE_INSTALL_PREFIX}/${plugin_dest_dir}/platforms")
	SET(APPS "${CMAKE_INSTALL_PREFIX}/${UX3D_OUTPUT_NAME}.app")
ENDIF(APPLE)
IF(WIN32)
	SET(APPS "\${CMAKE_INSTALL_PREFIX}/bin/${UX3D_OUTPUT_NAME}.exe")
	SET(frameworks_dest_dir "${CMAKE_INSTALL_PREFIX}/bin")
	#SET(plugin_dest_dir "${CMAKE_INSTALL_PREFIX}/bin")
	#SET(qtconf_dest_dir "${CMAKE_INSTALL_PREFIX}/bin")
	#SET(resources_dest_dir "${CMAKE_INSTALL_PREFIX}/bin")
	#SET(qt_platforms_plugin_dir "${plugin_dest_dir}/platforms")
ENDIF(WIN32)
if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
	SET(frameworks_dest_dir "${CMAKE_INSTALL_PREFIX}/lib")
	SET(plugin_dest_dir "${CMAKE_INSTALL_PREFIX}/bin")
	SET(qtconf_dest_dir "${CMAKE_INSTALL_PREFIX}/bin")
	SET(resources_dest_dir "${CMAKE_INSTALL_PREFIX}/bin")
	SET(qt_platforms_plugin_dir "${plugin_dest_dir}/platforms")
endif()

# Install the application, on Apple, the bundle is at the root of the
# install tree, and on other platforms it'll go into the bin directory.
INSTALL(TARGETS ${PROJECT_NAME}
	BUNDLE DESTINATION . COMPONENT Runtime
	RUNTIME DESTINATION bin COMPONENT Runtime
	LIBRARY DESTINATION lib COMPONENT Runtime
	CONFIGURATIONS Release
)

# Install the icon and desktop for ubuntu
if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
    install(FILES ${PROJECT_SOURCE_DIR}/resources/Gestaltor_AppIcon_1024.png DESTINATION ${CMAKE_INSTALL_PREFIX}/../ COMPONENT Runtime)
	install(CODE "set(UX3D_PROJECT_NAME \"${UX3D_PROJECT_NAME}\")" COMPONENT Runtime)
	install(CODE "set(UX3D_OUTPUT_NAME \"${UX3D_OUTPUT_NAME}\")" COMPONENT Runtime)
    install(CODE "configure_file(\"${PROJECT_SOURCE_DIR}/resources/gestaltor.desktop.in\" ${CMAKE_INSTALL_PREFIX}/../gestaltor.desktop)" COMPONENT Runtime)
	install(CODE "execute_process( \
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    usr/bin/${UX3D_OUTPUT_NAME} \
    ${CMAKE_INSTALL_PREFIX}/../AppRun   \
    )"
    COMPONENT Runtime)
endif()

set(USD_DIR "${PROJECT_SOURCE_DIR}/../gestaltor_deps/${CMAKE_GENERATOR}/usd_repo-src")

# Install needed Qt plugins by copying directories from the qt installation
# One can cull what gets copied by using 'REGEX "..." EXCLUDE'
IF(APPLE)
    INSTALL(FILES "${QTBASE_PATH}/plugins/platforms/libqcocoa.dylib" DESTINATION ${plugin_dest_dir}/platforms COMPONENT Runtime)
    
    # set USD parameters
	set(USD_LIBRARY_DIR "${USD_DIR}/lib")
	INSTALL(DIRECTORY "${USD_LIBRARY_DIR}/usd" DESTINATION ${frameworks_dest_dir} COMPONENT Runtime)
ENDIF()

if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
	INSTALL(DIRECTORY "${QTBASE_PATH}/plugins/platforms/"
		DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins/platforms
		COMPONENT Runtime)
	INSTALL(DIRECTORY
			"${QTBASE_PATH}/plugins/xcbglintegrations/"
		DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins/xcbglintegrations
		COMPONENT Runtime)
    INSTALL(DIRECTORY ${CMAKE_BINARY_DIR}/lib DESTINATION . COMPONENT Runtime)

    # set USD parameters
    file(GLOB usd_libs "${USD_DIR}/lib/*.dll")
    file(COPY ${usd_libs} DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
    
	set(USD_LIBRARY_DIR "${USD_DIR}/lib")
	INSTALL(DIRECTORY "${USD_LIBRARY_DIR}/usd" DESTINATION ${frameworks_dest_dir} COMPONENT Runtime)
endif()

# install the qml sources

# install a qt.conf file
# this inserts some cmake code into the install script to write the file
INSTALL(CODE "
	file(WRITE \"${qtconf_dest_dir}/qt.conf\" \"\")
	" COMPONENT Runtime)


# get Qt bin dir
get_target_property (QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_PATH ${QT_QMAKE_EXECUTABLE} DIRECTORY)

# directories to look for dependencies
# TODO: the release output dir is IDE specific, somehow the CMAKE_RUNTIME_OUTPUT_DIRECTORY_<CONFIG> variable is not set
SET(DIRS 
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} 
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release
    ${CMAKE_BINARY_DIR}/lib
    ${USD_LIBRARY_DIR}
    )

if(APPLE)
    # no additional app-store framework dependencies
endif()

if (NOT APPLE)
	# This is not required when all dependencies are copied/ built to the correct location
	# by the IDE / while building
	# This can be done for other platforms than apple as well
	add_custom_command(TARGET ${PROJECT_NAME}
	COMMAND ${CMAKE_COMMAND}
			-DBUILD_CONFIG_DIR=${CMAKE_CFG_INTDIR}
			-DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
			-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
			-DPLUGIN_DEST_DIR=${plugin_dest_dir}
			-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
			-DCMAKE_GENERATOR=${CMAKE_GENERATOR}
			-DQT_BIN_PATH=${QT_BIN_PATH}
			-DCV_PATH=${cv_path}
			-P "${PROJECT_SOURCE_DIR}/cmake/copy_post_build.cmake"
	VERBATIM
	)
endif()

if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
    message("${QTBASE_PATH} ${QTDECLARATIVE_PATH} ${QT5COMPAT_PATH} ${QTSVG_PATH} ${QTTOLS_PATH}")
	execute_process(COMMAND mkdir "${CMAKE_BINARY_DIR}/qt")
	execute_process(COMMAND chmod u+x ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dircontent_if_different.sh)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dircontent_if_different.sh ${QTBASE_PATH} ${CMAKE_BINARY_DIR}/qt/)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dircontent_if_different.sh ${QTDECLARATIVE_PATH} ${CMAKE_BINARY_DIR}/qt/)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dircontent_if_different.sh ${QT5COMPAT_PATH} ${CMAKE_BINARY_DIR}/qt/)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dircontent_if_different.sh ${QTSVG_PATH} ${CMAKE_BINARY_DIR}/qt/)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dircontent_if_different.sh ${QTTOOLS_PATH} ${CMAKE_BINARY_DIR}/qt/)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dircontent_if_different.sh ${QTSTATEMACHINEQML_PATH} ${CMAKE_BINARY_DIR}/qt/)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dircontent_if_different.sh ${QTSTATEMACHINE_PATH} ${CMAKE_BINARY_DIR}/qt/)

	list(APPEND DIRS ${CMAKE_BINARY_DIR}/qt/lib)

	list(APPEND IGNORE_ITEMS "${gltf-validator_SOURCE_DIR}/build/bin/gltf_validator")
    INSTALL(CODE "
	include(BundleUtilities)
	fixup_bundle(\"${APPS}\" \"${LIBS}\" \"${DIRS}\" IGNORE_ITEM \"${IGNORE_ITEMS}\")
	" COMPONENT Runtime)

	INSTALL(
    PROGRAMS "${gltf-validator_SOURCE_DIR}/build/bin/gltf_validator" 
    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" 
    COMPONENT Runtime
	)
	FILE(COPY "${gltf-validator_SOURCE_DIR}/build/bin/gltf_validator" 
	DESTINATION "${CMAKE_BINARY_DIR}/bin")

endif()

if(APPLE)

	INSTALL(CODE "
		execute_process(COMMAND \"${QTBASE_PATH}/bin/macdeployqt\" \"${CMAKE_INSTALL_PREFIX}/${UX3D_OUTPUT_NAME}.app\" \"-qmldir=${PROJECT_SOURCE_DIR}/editor/qml\")
	" COMPONENT Runtime)

	INSTALL(CODE "
		include(BundleUtilities)
		fixup_bundle(\"${APPS}\" \"${LIBS}\" \"${DIRS}\" IGNORE_ITEM \"${IGNORE_ITEMS}\")
		" COMPONENT Runtime)
endif()

# This adds missing Qt libs to the output directory in Release on Windows
if(WIN32)
 	#add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}> COMMAND_EXPAND_LISTS)
	message("Copy Qt to build folder")
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dir_if_different.bat ${QTBASE_PATH} ${CMAKE_BINARY_DIR}/qt)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dir_if_different.bat ${QTDECLARATIVE_PATH} ${CMAKE_BINARY_DIR}/qt)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dir_if_different.bat ${QT5COMPAT_PATH} ${CMAKE_BINARY_DIR}/qt)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dir_if_different.bat ${QTSVG_PATH} ${CMAKE_BINARY_DIR}/qt)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dir_if_different.bat ${QTTOOLS_PATH} ${CMAKE_BINARY_DIR}/qt)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dir_if_different.bat ${QTINSTALLERFRAMEWORK_PATH} ${CMAKE_BINARY_DIR}/qt)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dir_if_different.bat ${QTSTATEMACHINE_PATH} ${CMAKE_BINARY_DIR}/qt)
	execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_dir_if_different.bat ${QTSTATEMACHINEQML_PATH} ${CMAKE_BINARY_DIR}/qt)

	if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug" OR(NOT EXISTS "${CMAKE_BINARY_DIR}/bin/Qt6QuickWidgetsd.dll" AND NOT EXISTS "${CMAKE_BINARY_DIR}/bin/Debug/Qt6QuickWidgetsd.dll"))
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND "${CMAKE_BINARY_DIR}/qt/bin/windeployqt.exe" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/${UX3D_OUTPUT_NAME}.exe" "-qmldir=${PROJECT_SOURCE_DIR}/editor/qml" VERBATIM)
	endif()
endif()
endif() # GESTALTOR_BUILD_QT

#--------------------------------------------------------------------------------
# Standalone Dear ImGui Target
#--------------------------------------------------------------------------------
if (GESTALTOR_BUILD_IMGUI)
	set(IMGUI_APP_NAME "Gestaltor-ImGui")

	set(IMGUI_SOURCES
		"source/imgui/main.cpp"
		"source/imgui/GladGL.cpp"
		"header/imgui/GladGL.h"
		"source/imgui/Framebuffer.cpp"
		"header/imgui/Framebuffer.h"
		"source/imgui/OrbitCamera.cpp"
		"header/imgui/OrbitCamera.h"
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
		"source/imgui/ResourcesPanel.cpp"
		"header/imgui/ResourcesPanel.h"
		"source/imgui/ElementInspector.cpp"
		"header/imgui/ElementInspector.h"
		"source/imgui/AnimationPlayer.cpp"
		"header/imgui/AnimationPlayer.h"
		"source/imgui/AntiAliasing.cpp"
		"header/imgui/AntiAliasing.h"
	)

	add_executable(${IMGUI_APP_NAME} ${IMGUI_SOURCES})

	target_include_directories(${IMGUI_APP_NAME} PRIVATE
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

	target_link_libraries(${IMGUI_APP_NAME} PRIVATE
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
		Reactor
		Audio
		Core
		Math
		Logging
		subprocess
		draco::draco
		nlohmann_json::nlohmann_json
		TBB::tbb
		opengl32
	)

	set_target_properties(${IMGUI_APP_NAME} PROPERTIES
		OUTPUT_NAME "Gestaltor"
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
	)

	add_custom_command(TARGET ${IMGUI_APP_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${PROJECT_SOURCE_DIR}/OpenGL-Renderer/resources"
			"$<TARGET_FILE_DIR:${IMGUI_APP_NAME}>/resources"
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${PROJECT_SOURCE_DIR}/resources"
			"$<TARGET_FILE_DIR:${IMGUI_APP_NAME}>/resources"
		COMMENT "Copying runtime resources next to Gestaltor executable"
	)
endif()



#set(CPACK_BINARY_DRAGNDROP ON)
#include(CPack)

