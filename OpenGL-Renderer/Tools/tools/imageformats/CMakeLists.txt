set(lib_name "ImageFormats")

add_sources("source/*.cpp" "target_sources")
add_sources("include/imageformats/*.h" "target_sources")

add_library(${lib_name} ${UX3D_MODULE_LINKAGE} "${target_sources}")
target_include_directories(${lib_name} PUBLIC "include")

find_package(stb)
target_link_libraries(${lib_name} PUBLIC Core)
target_link_libraries(${lib_name} PUBLIC Image)
target_link_libraries(${lib_name} PUBLIC Filesystem)
target_link_libraries(${lib_name} PRIVATE stb::stb)

find_package(Ktx)

target_link_libraries(${lib_name} PRIVATE KTX::ktx)

find_package(WebP REQUIRED)
target_link_libraries(${lib_name} PRIVATE WebP::WebP)

if (${UX3D_TOOLS_TESTING})
	add_sources("tests/*.cpp" "test_sources")
	add_library(${lib_name}Tests OBJECT "${test_sources}")
	find_package(catch2 REQUIRED)
	target_include_directories(${lib_name}Tests PUBLIC "${catch2_INCLUDE_DIRS}")
	target_link_libraries(${lib_name}Tests PUBLIC ${lib_name} TestHelpers)
	set_directory_structure(${lib_name}Tests)

	set(test_libs ${test_libs};${lib_name}Tests CACHE INTERNAL "")
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

install(TARGETS ${lib_name} FRAMEWORK DESTINATION "lib" COMPONENT Runtime CONFIGURATIONS Release)
