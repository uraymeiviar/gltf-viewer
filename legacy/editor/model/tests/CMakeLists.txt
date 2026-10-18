# ######################################
# # Download and install GoogleTest
# ######################################

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.11.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# ######################################
# # Create tests executable
# ######################################

add_sources("*.cpp" "test_sources")
add_sources("*.h" "test_sources")
add_executable(${lib_name}_tests "${test_sources}")

target_link_libraries(${lib_name}_tests gtest)
target_link_libraries(${lib_name}_tests gtest_main)
target_link_libraries(${lib_name}_tests gmock)
target_link_libraries(${lib_name}_tests gmock_main)
target_link_libraries(${lib_name}_tests ${lib_name})

# ######################################
# # Add threads for multithreaded testing
# ######################################

find_package(Threads REQUIRED)
target_link_libraries(${lib_name}_tests Threads::Threads)

######################################
# Make the test runnable
######################################

include(CTest)
add_test(NAME ${lib_name}_tests COMMAND ${lib_name}_tests)