add_library(TestHelpers "TestFileSystem.cpp" "include/testHelpers/TestFileSystem.h")
target_include_directories(TestHelpers PUBLIC "include")
target_link_libraries(TestHelpers Filesystem)
set_directory_structure(TestHelpers)