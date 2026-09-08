add_compile_options(-w)

option(BUILD_EXTERNAL "Build external dependencies in /External" OFF)
option(SKIP_GLSLANG_INSTALL "Skip installation" ON)
set(ENABLE_GLSLANG_INSTALL OFF)
option(ENABLE_SPVREMAPPER "Enables building of SPVRemapper" OFF)
option(ENABLE_GLSLANG_BINARIES "Builds glslangValidator and spirv-remap" OFF)

option(ENABLE_GLSLANG_WEB "Reduces glslang to minimum needed for web use" OFF)
option(ENABLE_GLSLANG_WEB_DEVEL "For ENABLE_GLSLANG_WEB builds, enables compilation error messages" OFF)
option(ENABLE_EMSCRIPTEN_SINGLE_FILE "If using Emscripten, enables SINGLE_FILE build" OFF)
option(ENABLE_EMSCRIPTEN_ENVIRONMENT_NODE "If using Emscripten, builds to run on Node instead of Web" OFF)

option(ENABLE_HLSL "Enables HLSL input support" OFF)

add_definitions(-w)

include(stduuid.cmake)
include(GSL.cmake)
