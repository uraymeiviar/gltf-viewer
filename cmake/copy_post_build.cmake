### On windows copy dlls from e.g. Debug to Debug/bin (this happens for e.g. ktx because of multi-config)
if(EXISTS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}" AND NOT "${BUILD_CONFIG_DIR}" STREQUAL "." )
	file(GLOB copy_from_bin "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/*.dll")
	file(COPY ${copy_from_bin} DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
endif()

### On macOS copy ktx dylibs from lib to bin/Debug and bin/Release
if(EXISTS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}" AND NOT "${BUILD_CONFIG_DIR}" STREQUAL "." )
	file(GLOB copy_from_bin "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/*.dylib")
	file(COPY ${copy_from_bin} DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
endif()

### Copy runtime dependencies to qt folder (workaround for deployqt)
file (GLOB copy_to_qt "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}/*keychain*")
file (COPY ${copy_to_qt} DESTINATION "${QT_BIN_PATH}")

### Copy usd and ufg
if(WIN32)
	message(STATUS "Copying usd files to bin")
    file(COPY "${PROJECT_SOURCE_DIR}/../gestaltor_deps/${CMAKE_GENERATOR}/usd_repo-src/lib/usd" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
    file(GLOB usd_libs "${PROJECT_SOURCE_DIR}/../gestaltor_deps/${CMAKE_GENERATOR}/usd_repo-src/lib/*.dll")
    file(GLOB usd_libs2 "${PROJECT_SOURCE_DIR}/../gestaltor_deps/${CMAKE_GENERATOR}/usd_repo-src/bin/*.dll")
    file(COPY ${usd_libs} DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
    file(COPY ${usd_libs2} DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
    file(COPY "${PROJECT_SOURCE_DIR}/../gestaltor_deps/${CMAKE_GENERATOR}/usd_from_gltf_repo-src/bin/usd_from_gltf.exe" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
    file(COPY "${PROJECT_SOURCE_DIR}/../gestaltor_deps/${CMAKE_GENERATOR}/gltf-validator-src/build/bin/gltf_validator.exe" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
    message(STATUS "Copy finished")
endif()