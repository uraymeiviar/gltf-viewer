set(lib_name "Operations")

add_sources("source/*.cpp" "target_sources")
add_sources("source/*.h" "target_sources")

add_sources("source/mikktspace/*.cpp" "target_sources")
add_sources("source/mikktspace/*.h" "target_sources")

add_sources("include/operations/*.h" "target_sources")

add_library(${lib_name} ${UX3D_MODULE_LINKAGE} "${target_sources}")
target_include_directories(${lib_name} PUBLIC "include")


# add dependencies
find_package(nlohmann_json)
target_link_libraries(${lib_name} PUBLIC nlohmann_json::nlohmann_json)
FIND_PACKAGE(nlohmann_json_schema_validator)
target_link_libraries(${lib_name} PUBLIC nlohmann_json_schema_validator)
target_link_libraries(${lib_name} PUBLIC Reactor)
target_link_libraries(${lib_name} PUBLIC DataStorage)
target_link_libraries(${lib_name} PUBLIC gltf-schema)
target_link_libraries(${lib_name} PUBLIC ImageFormats)
target_link_libraries(${lib_name} PUBLIC Utils)
find_package(draco REQUIRED)
target_link_libraries(${lib_name} PRIVATE draco::draco)
find_package(assimp REQUIRED)
target_link_libraries(${lib_name} PRIVATE assimp::assimp)
find_package(Ktx)
target_link_libraries(${lib_name} PRIVATE KTX::ktx)
find_package(fmt REQUIRED)
target_link_libraries(${lib_name} PRIVATE fmt::fmt)
target_link_libraries(${lib_name} PRIVATE subprocess)

target_include_directories(${lib_name} PRIVATE ${draco_INCLUDE_DIRS})

set_target_properties(${lib_name} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

if (${UX3D_TOOLS_TESTING})
	add_subdirectory(tests)
endif()

if (UX3D_USE_IWYU)
	find_program(iwyu_path NAMES "include-what-you-use")
	if(iwyu_path)
		set_property(TARGET ${lib_name} PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
	endif()
endif()

set_directory_structure(${lib_name})

make_framework(
    TARGET ${lib_name} 
    VERSION 1.0.0 
    IDENTIFIER io.ux3d.${lib_name} 
    PUBLIC_HEADER ${target_headers}
)

# TODO: enable once DataStorage is part of the Gestaltor project
# install(TARGETS ${lib_name} COMPONENT Runtime CONFIGURATIONS Release)
