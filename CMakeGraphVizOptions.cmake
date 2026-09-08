# put this file in the top source dir (CMAKE_SOURCE_DIR)
# see: https://cmake.org/cmake/help/latest/module/CMakeGraphVizOptions.html

set(GRAPHVIZ_EXTERNAL_LIBS FALSE)

set(GRAPHVIZ_EXECUTABLES TRUE)

set(GRAPHVIZ_IGNORE_TARGETS "Tests$" "_tests$"  Async P2G2 json "^rcc" SPIRV glslang OGLCompiler OSDependent TestHelpers pbr2gltf2 assimp_cmd errors "^issue-" string-format-check-test uri gtest gtest_main assimp RxCpp GSL stduuid)
