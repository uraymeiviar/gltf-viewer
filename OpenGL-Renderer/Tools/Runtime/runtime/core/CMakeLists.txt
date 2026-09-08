set(lib_name "Core")

add_sources("source/*.cpp" target_sources)
add_sources("include/core/*.h" target_headers)
add_include_folder("include" "target_includes")

add_library(${lib_name} ${UX3D_MODULE_LINKAGE} ${target_sources} ${target_headers})
target_include_directories(${lib_name} PUBLIC "${target_includes}")
target_link_libraries(${lib_name} PUBLIC RxCpp)

find_package(nlohmann_json)
target_link_libraries(${lib_name} PUBLIC nlohmann_json::nlohmann_json)

if (${UX3D_RUNTIME_TESTING})
	add_sources("tests/*.cpp" "test_sources")
	add_library(${lib_name}Tests OBJECT "${test_sources}")
	find_package(catch2 REQUIRED)
	target_include_directories(${lib_name}Tests PUBLIC "${catch2_INCLUDE_DIRS}")
	target_link_libraries(${lib_name}Tests PUBLIC ${lib_name})
	set_directory_structure(${lib_name}Tests)

	set(${PROJECT_NAME}_test_libs ${${PROJECT_NAME}_test_libs};${lib_name}Tests CACHE INTERNAL "")
endif()

set_directory_structure(${lib_name})

if (UX3D_USE_IWYU)
	find_program(iwyu_path NAMES "include-what-you-use")
	if(iwyu_path)
		set_property(TARGET ${lib_name} PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
	endif()
endif()

make_framework(
    TARGET ${lib_name} 
    VERSION 1.0.0 
    IDENTIFIER io.ux3d.${lib_name} 
    PUBLIC_HEADER ${target_headers}
)

install(TARGETS ${lib_name} FRAMEWORK DESTINATION "lib" COMPONENT Runtime CONFIGURATIONS Release)
