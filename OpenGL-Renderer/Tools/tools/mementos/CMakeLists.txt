set(lib_name "Mementos")

set(target_sources "${CMAKE_CURRENT_SOURCE_DIR}/include/mementos/Mementos.h" )

add_library(${lib_name} INTERFACE)
target_sources(${lib_name} INTERFACE ${target_sources})
target_include_directories(${lib_name} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${lib_name} INTERFACE RxCpp)

if (${UX3D_TOOLS_TESTING})
	add_sources("tests/*.cpp" "test_sources")
	add_library(${lib_name}Tests OBJECT "${test_sources}")
	find_package(catch2 REQUIRED)
	target_include_directories(${lib_name}Tests PUBLIC "${catch2_INCLUDE_DIRS}")
	target_link_libraries(${lib_name}Tests PUBLIC ${lib_name} TestHelpers)

	find_package(nlohmann_json)
	target_link_libraries(${lib_name}Tests PUBLIC nlohmann_json::nlohmann_json)
	set_directory_structure(${lib_name}Tests)

	set(test_libs ${test_libs};${lib_name}Tests CACHE INTERNAL "")
endif()

if (UX3D_USE_IWYU)
	find_program(iwyu_path NAMES "include-what-you-use")
	if(iwyu_path)
		set_property(TARGET ${lib_name} PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
	endif()
endif()
