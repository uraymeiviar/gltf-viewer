set(lib_name "model")

if(MSVC)
	add_compile_options(/bigobj)
endif()

add_subdirectory(thirdparty)
add_subdirectory(tasks)

add_sources("source/*.cpp" "target_sources")
add_sources("include/model/*.h" "target_sources")
add_include_folder("include" "target_includes")

set(linkage STATIC)
if(UX3D_DYNAMIC_LINKAGE)
	set(linkage SHARED)
endif()

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)


add_library(${lib_name} ${linkage} "${target_sources}")

target_include_directories(${lib_name} PUBLIC "${target_includes}")
target_link_libraries(${lib_name} PUBLIC Core)
target_link_libraries(${lib_name} PUBLIC Filesystem)
target_link_libraries(${lib_name} PUBLIC Gltf)
target_link_libraries(${lib_name} PUBLIC Tasks)
target_link_libraries(${lib_name} PUBLIC Mementos)
target_link_libraries(${lib_name} PUBLIC DataStorage)
target_link_libraries(${lib_name} PUBLIC RxCpp)
target_link_libraries(${lib_name} PUBLIC Reactor)
target_link_libraries(${lib_name} PUBLIC Operations)
target_link_libraries(${lib_name} PUBLIC gltf-schema)
target_link_libraries(${lib_name} PUBLIC ImageFormats)

if (${UX3D_TOOLS_TESTING})
	add_subdirectory(tests)
endif()

GroupSourcesByFolder(${lib_name})

if (UX3D_USE_IWYU)
	find_program(iwyu_path NAMES "include-what-you-use")
	if(iwyu_path)
		set_property(TARGET ${lib_name} PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
	endif()
endif()

install(TARGETS ${lib_name} COMPONENT Runtime CONFIGURATIONS Release)