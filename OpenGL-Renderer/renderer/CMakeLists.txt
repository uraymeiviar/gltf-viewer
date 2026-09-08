add_sources("source/*.cpp" "target_sources")
add_sources("source/gl/*.cpp" "target_sources")
add_sources("source/gltf/*.cpp" "target_sources")
add_sources("source/passes/*.cpp" "target_sources")
add_sources("source/editor/*.cpp" "target_sources")
add_sources("source/filtering/*.cpp" "target_sources")

add_sources("include/renderer/*.h" "target_headers")
add_sources("include/renderer/gl/*.h" "target_headers")
add_sources("include/renderer/passes/*.h" "target_headers")
add_sources("include/renderer/gltf/*.h" "target_headers")
add_sources("include/renderer/editor/*.h" "target_headers")
add_sources("include/renderer/enums/*.h" "target_headers")
add_sources("include/renderer/filtering/*.h" "target_headers")

add_include_folder("include" "target_includes")

find_package(OpenAL)

add_library(OpenGL-Renderer ${UX3D_MODULE_LINKAGE} ${target_sources} ${target_headers})

target_include_directories(OpenGL-Renderer PUBLIC "${target_includes}")
target_link_libraries(OpenGL-Renderer PUBLIC Core)
target_link_libraries(OpenGL-Renderer PUBLIC Math)
target_link_libraries(OpenGL-Renderer PUBLIC Logging)
target_link_libraries(OpenGL-Renderer PUBLIC Image)
target_link_libraries(OpenGL-Renderer PUBLIC Animation)
target_link_libraries(OpenGL-Renderer PUBLIC Gltf)
target_link_libraries(OpenGL-Renderer PUBLIC Filesystem)
target_link_libraries(OpenGL-Renderer PRIVATE ImageFormats)
target_link_libraries(OpenGL-Renderer PUBLIC OpenAL::OpenAL)

set_directory_structure(OpenGL-Renderer)

make_framework(
    TARGET OpenGL-Renderer 
    VERSION 1.0.0 
    IDENTIFIER io.ux3d.OpenGL-Renderer
    PUBLIC_HEADER ${target_headers}
)

install(TARGETS OpenGL-Renderer FRAMEWORK DESTINATION "lib" COMPONENT Runtime CONFIGURATIONS Release)
