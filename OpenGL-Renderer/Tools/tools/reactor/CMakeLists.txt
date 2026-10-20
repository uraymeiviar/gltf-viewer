set(lib_name "Reactor")

add_sources("source/*.cpp" "target_sources")
add_sources("source/*.h" "target_sources")
add_sources("include/reactor/*.h" "target_sources")

add_library(${lib_name} ${UX3D_MODULE_LINKAGE} "${target_sources}")
target_include_directories(${lib_name} PUBLIC "include")

find_package(stb)

# add dependencies
target_link_libraries(${lib_name} PUBLIC RxCpp)
find_package(nlohmann_json)
target_link_libraries(${lib_name} PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(${lib_name} PUBLIC GSL)
target_link_libraries(${lib_name} PUBLIC stduuid)
FIND_PACKAGE(nlohmann_json_schema_validator)
target_link_libraries(${lib_name} PUBLIC nlohmann_json_schema_validator)
target_link_libraries(${lib_name} PUBLIC DataStorage)
find_package(Ktx)
target_link_libraries(${lib_name} PUBLIC KTX::ktx)
target_link_libraries(${lib_name} PRIVATE stb::stb)
find_package(glm)
target_include_directories(${lib_name} SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/../Tools/Runtime/thirdparty/glm/glm)

find_package(WebP REQUIRED)
target_link_libraries(${lib_name} PUBLIC WebP::WebP)


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
