set(lib_name "Tasks")

add_sources("source/*.cpp" "target_sources")
add_sources("include/model/tasks/*.h" "target_headers")
add_include_folder("include" "target_includes")


set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

add_library(${lib_name} ${UX3D_MODULE_LINKAGE} ${target_sources} ${target_headers})

target_include_directories(${lib_name} PUBLIC "${target_includes}")

target_link_libraries(${lib_name} PUBLIC Filesystem)
target_link_libraries(${lib_name} PUBLIC ImageFormats)
target_link_libraries(${lib_name} PUBLIC Math)

make_framework(
    TARGET ${lib_name} 
    VERSION 1.0.0 
    IDENTIFIER io.ux3d.${lib_name} 
    PUBLIC_HEADER ${target_headers}
)

install(TARGETS ${lib_name} FRAMEWORK DESTINATION "lib" COMPONENT Runtime CONFIGURATIONS Release)
