set(lib_name "Gltf")

add_sources("source/*.cpp" sources)
add_sources("source/json/*.cpp" sources)
add_sources("source/material/*.cpp" sources)
add_sources("source/animation/*.cpp" sources)
add_sources("source/merge/*.cpp" sources)
add_sources("source/mesh/*.cpp" sources)
add_sources("source/accessor/*.cpp" sources)
add_sources("source/extensions/*.cpp" sources)
add_sources("source/mikktspace/*.c" sources)
add_sources("source/mikktspace/*.cpp" sources)

add_sources("include/gltf/*.h" headers)
add_sources("include/gltf/json/*.h" headers)
add_sources("include/gltf/material/*.h" headers)
add_sources("include/gltf/animation/*.h" headers)
add_sources("include/gltf/merge/*.h" headers)
add_sources("include/gltf/mesh/*.h" headers)
add_sources("include/gltf/accessor/*.h" headers)
add_sources("include/gltf/extensions/*.h" headers)
add_sources("include/gltf/mikktspace/*.h" headers)

add_library(${lib_name} ${UX3D_MODULE_LINKAGE} "${sources};${headers}")

target_include_directories(${lib_name} PUBLIC "include")

find_package(nlohmann_json)
target_link_libraries(${lib_name} PUBLIC nlohmann_json::nlohmann_json)

target_link_libraries(${lib_name} PUBLIC Core)
target_link_libraries(${lib_name} PUBLIC Math)
target_link_libraries(${lib_name} PUBLIC Filesystem)
target_link_libraries(${lib_name} PUBLIC ImageFormats)
target_link_libraries(${lib_name} PUBLIC Utils)
target_link_libraries(${lib_name} PUBLIC Mementos)
target_link_libraries(${lib_name} PUBLIC DataStorage)
target_link_libraries(${lib_name} PUBLIC Reactor)
target_link_libraries(${lib_name} PUBLIC Audio)

FIND_PACKAGE(draco)

target_link_libraries(${lib_name} PRIVATE draco::draco)
target_include_directories(${lib_name} PRIVATE ${draco_INCLUDE_DIRS})

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
