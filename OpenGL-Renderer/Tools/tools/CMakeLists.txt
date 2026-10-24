add_subdirectory(filesystem)
add_subdirectory(imageformats)
add_subdirectory(logging)
add_subdirectory(testHelpers)
add_subdirectory(utils)
add_subdirectory(datastorage)
add_subdirectory(mementos)
add_subdirectory(gltf)
add_subdirectory(reactor)
# operations/ (document edit operations from the Gestaltor editor) is not configured: Glider does
# not use it, and it needs the glTF-Schema generated headers, which are no longer in the repository.

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	target_compile_options(Filesystem PRIVATE "-Wall" "-Wextra" "-Wconversion" "-Wno-unused-parameter")
	target_compile_options(Gltf PRIVATE "-Wall" "-Wextra" "-Wconversion" "-Wno-unused-parameter")
	target_compile_options(ImageFormats PRIVATE "-Wall" "-Wextra" "-Wconversion" "-Wno-unused-parameter")
	target_compile_options(Logging PRIVATE "-Wall" "-Wextra" "-Wconversion" "-Wno-unused-parameter")
endif()
