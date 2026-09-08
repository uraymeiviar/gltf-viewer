include("${ux3d_utils}")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	add_compile_options("-Wall")
	add_compile_options("-Wextra")
endif()

if (UX3D_USE_CLANG_TIDY)
	set(CMAKE_CXX_CLANG_TIDY clang-tidy)
endif ()

add_subdirectory(core)
add_subdirectory(image)
add_subdirectory(audio)
add_subdirectory(math)
add_subdirectory(animation)
add_subdirectory(gpu)
