set(lib_name "Utils")

add_sources("source/*.cpp" "target_sources")
add_sources("include/utils/*.h" "target_sources")

add_library(${lib_name} ${UX3D_MODULE_LINKAGE} "${target_sources}")
target_include_directories(${lib_name} PUBLIC "include")

target_link_libraries(${lib_name} PUBLIC Core)
target_link_libraries(${lib_name} PUBLIC Math)

set_target_properties(${lib_name} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)


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
