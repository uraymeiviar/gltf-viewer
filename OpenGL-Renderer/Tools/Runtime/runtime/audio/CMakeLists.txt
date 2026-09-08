add_sources("source/*.cpp" "target_sources")
add_sources("include/audio/*.h" "target_sources")
add_include_folder("include" "target_includes")

find_package(minimp3)

add_library(Audio ${UX3D_MODULE_LINKAGE} "${target_sources}")
target_include_directories(Audio PUBLIC "${target_includes}")

target_link_libraries(Audio PRIVATE Core)
set_directory_structure(Audio)

target_link_libraries(Audio PRIVATE minimp3::minimp3)

if (UX3D_USE_IWYU)
	find_program(iwyu_path NAMES "include-what-you-use")
	if(iwyu_path)
		set_property(TARGET Audio PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
	endif()
endif()

make_framework(
    TARGET Audio 
    VERSION 1.0.0 
    IDENTIFIER io.ux3d.Audio 
    PUBLIC_HEADER ${target_headers}
)

install(TARGETS Audio FRAMEWORK DESTINATION "lib" COMPONENT Runtime CONFIGURATIONS Release)
