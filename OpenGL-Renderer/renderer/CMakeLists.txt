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

# Per-command GL error checks are compiled out in Release by default. Debug
# builds keep them, and this option restores them for release diagnostics.
if (GLIDER_GL_DEBUG_CHECKS)
    target_compile_definitions(OpenGL-Renderer PRIVATE GLIDER_GL_DEBUG_CHECKS=1)
endif()

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
