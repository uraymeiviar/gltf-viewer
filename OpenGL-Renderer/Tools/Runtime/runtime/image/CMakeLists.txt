add_sources("source/*.cpp" target_sources)
add_sources("include/image/*.h" target_headers)
add_include_folder("include" "target_includes")

add_library(Image ${UX3D_MODULE_LINKAGE} ${target_sources} ${target_headers})
target_include_directories(Image PUBLIC "${target_includes}")

target_link_libraries(Image PRIVATE Core)
target_link_libraries(Image PUBLIC Math)
target_link_libraries(Image PUBLIC Gpu)
set_directory_structure(Image)

if (UX3D_USE_IWYU)
	find_program(iwyu_path NAMES "include-what-you-use")
	if(iwyu_path)
		set_property(TARGET Image PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
	endif()
endif()

make_framework(
    TARGET Image 
    VERSION 1.0.0 
    IDENTIFIER io.ux3d.Image 
    PUBLIC_HEADER ${target_headers}
)

install(TARGETS Image FRAMEWORK DESTINATION "lib" COMPONENT Runtime CONFIGURATIONS Release)