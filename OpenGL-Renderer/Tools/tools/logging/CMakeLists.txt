add_sources("source/*.cpp" "target_sources")
add_sources("include/logging/*.h" "target_sources")
add_include_folder("include" "target_includes")

add_library(Logging ${UX3D_MODULE_LINKAGE} "${target_sources}")
target_include_directories(Logging PUBLIC "${target_includes}")
target_link_libraries(Logging PRIVATE Core Filesystem)

if (UX3D_USE_IWYU)
	find_program(iwyu_path NAMES "include-what-you-use")
	if(iwyu_path)
		set_property(TARGET Logging PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
	endif()
endif()

if (${UX3D_TOOLS_TESTING})
	add_sources("tests/*.cpp" "test_sources")
	add_library(LoggingTests OBJECT "${test_sources}")
	find_package(catch2 REQUIRED)
	target_include_directories(LoggingTests PUBLIC "${catch2_INCLUDE_DIRS}")
	target_link_libraries(LoggingTests PUBLIC Logging TestHelpers)
	set_directory_structure(LoggingTests)

	set(test_libs ${test_libs};LoggingTests CACHE INTERNAL "")
endif()

set_directory_structure(Logging)

make_framework(
    TARGET Logging
    VERSION 1.0.0 
    IDENTIFIER io.ux3d.Logging 
    PUBLIC_HEADER ${target_headers}
)

install(TARGETS Logging FRAMEWORK DESTINATION "lib" COMPONENT Runtime CONFIGURATIONS Release)
