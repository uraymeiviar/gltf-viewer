add_sources("source/*.cpp" target_sources)
add_sources("include/tests/*.h" target_sources)

add_executable(${PROJECT_NAME}_Tests "${target_sources}")
target_include_directories(${PROJECT_NAME}_Tests PRIVATE "include")

# both are needed for logging (see tests.cpp):
target_link_libraries(${PROJECT_NAME}_Tests PRIVATE Core)
target_link_libraries(${PROJECT_NAME}_Tests PRIVATE Logging)

target_link_libraries(${PROJECT_NAME}_Tests PRIVATE ${test_libs})

if (UX3D_TEST_LOGGING)
	target_compile_definitions(${PROJECT_NAME}_Tests PRIVATE UX3D_TEST_LOGGING=1)
endif()

find_package(catch2 REQUIRED)
target_compile_definitions(${PROJECT_NAME}_Tests PRIVATE CATCH_CONFIG_EXTERNAL_INTERFACES)
target_include_directories(${PROJECT_NAME}_Tests PUBLIC "${catch2_INCLUDE_DIRS}")

set_directory_structure(${PROJECT_NAME}_Tests)
