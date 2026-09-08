add_sources("source/*.cpp" target_sources)
add_sources("include/tests/*.h" target_sources)

add_executable(${PROJECT_NAME}_Tests "${target_sources}")
target_include_directories(${PROJECT_NAME}_Tests PRIVATE "include")

target_link_libraries(${PROJECT_NAME}_Tests PRIVATE ${${PROJECT_NAME}_test_libs})

find_package(catch2 REQUIRED)
target_compile_definitions(${PROJECT_NAME}_Tests PRIVATE CATCH_CONFIG_EXTERNAL_INTERFACES)
target_include_directories(${PROJECT_NAME}_Tests PUBLIC "${catch2_INCLUDE_DIRS}")

enable_testing()
add_test(${PROJECT_NAME}_Tests ${PROJECT_NAME}_Tests)

set_directory_structure(${PROJECT_NAME}_Tests)
