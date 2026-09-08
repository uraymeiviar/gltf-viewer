add_sources("source/*.cpp" "target_sources")
add_sources("include/math/*.h" "target_sources")
add_include_folder("include" "target_includes")

add_library(Math ${UX3D_MODULE_LINKAGE} "${target_sources}")
target_include_directories(Math PUBLIC "${target_includes}")

#find_package(glm)
target_include_directories(Math SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/glm/glm)

target_link_libraries(Math PRIVATE Core)
set_directory_structure(Math)

if (UX3D_USE_IWYU)
	find_program(iwyu_path NAMES "include-what-you-use")
	if(iwyu_path)
		set_property(TARGET Math PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
	endif()
endif()

make_framework(
    TARGET Math 
    VERSION 1.0.0 
    IDENTIFIER io.ux3d.Math 
    PUBLIC_HEADER ${target_headers}
)

install(TARGETS Math FRAMEWORK DESTINATION "lib" COMPONENT Runtime CONFIGURATIONS Release)