set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

include(FetchContent)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/standalone_modules")

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

# 1. GLAD (OpenGL 4.1 Core profile)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../thirdparty/glad/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/glad" glad_build)
endif()

# 2. GLFW (Static Windowing library)
message(STATUS "[Standalone] Fetching GLFW...")
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 3. Dear ImGui (docking branch)
message(STATUS "[Standalone] Fetching Dear ImGui (docking branch)...")
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw glad)
endif()

# 4. ImGuizmo (3D Gizmos)
message(STATUS "[Standalone] Fetching ImGuizmo...")
FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG master
)
set(IMGUIZMO_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(imguizmo)
if(TARGET imguizmo)
    target_link_libraries(imguizmo PUBLIC imgui)
endif()

# 5. Portable File Dialogs (pfd)
message(STATUS "[Standalone] Fetching portable-file-dialogs...")
FetchContent_Declare(
    pfd
    GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
    GIT_TAG main
)
FetchContent_GetProperties(pfd)
if(NOT pfd_POPULATED)
    FetchContent_Populate(pfd)
    add_library(pfd INTERFACE)
    target_include_directories(pfd INTERFACE ${pfd_SOURCE_DIR})
endif()

# 6. nlohmann_json
message(STATUS "[Standalone] Fetching nlohmann_json...")
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# 7. fmt
message(STATUS "[Standalone] Fetching fmt...")
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 9.1.0
)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_DOC OFF CACHE BOOL "" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fmt)

# 8. stb
message(STATUS "[Standalone] Fetching stb...")
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 1ee679ca2ef753a528db5ba6801e1067b40481b8
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
    if(NOT TARGET stb::stb)
        add_library(stb::stb ALIAS stb)
    endif()
endif()

# 9. minimp3
message(STATUS "[Standalone] Fetching minimp3...")
FetchContent_Declare(
    minimp3
    GIT_REPOSITORY https://github.com/lieff/minimp3.git
    GIT_TAG master
)
FetchContent_GetProperties(minimp3)
if(NOT minimp3_POPULATED)
    FetchContent_Populate(minimp3)
    add_library(minimp3 INTERFACE)
    target_include_directories(minimp3 INTERFACE ${minimp3_SOURCE_DIR})
    if(NOT TARGET minimp3::minimp3)
        add_library(minimp3::minimp3 ALIAS minimp3)
    endif()
endif()

# 10. nlohmann_json_schema_validator
message(STATUS "[Standalone] Fetching json-schema-validator...")
FetchContent_Declare(
    nlohmann_json_schema_validator
    GIT_REPOSITORY https://github.com/pboettch/json-schema-validator.git
    GIT_TAG 2.3.0
)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json_schema_validator)

# 11. draco
message(STATUS "[Standalone] Fetching draco...")
FetchContent_Declare(
    draco
    GIT_REPOSITORY https://github.com/google/draco.git
    GIT_TAG 1.5.6
)
set(DRACO_TESTS OFF CACHE BOOL "" FORCE)
set(DRACO_ANIMATION_ENCODING ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(draco)
if(TARGET draco AND NOT TARGET draco::draco)
    add_library(draco::draco ALIAS draco)
endif()

# Isolate Draco standalone CLI tools away from distribution bin directory
foreach(draco_tool draco_encoder draco_decoder draco_transcoder draco_simplifier)
    if(TARGET ${draco_tool})
        set_target_properties(${draco_tool} PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tools/Release"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/tools/Debug"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/tools/RelWithDebInfo"
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/tools/MinSizeRel"
        )
    endif()
endforeach()


# 12. OpenAL Soft (Static)
message(STATUS "[Standalone] Fetching OpenAL Soft (Static)...")
FetchContent_Declare(
    openal
    GIT_REPOSITORY https://github.com/kcat/openal-soft.git
    GIT_TAG 1.23.1
)
set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
set(LIBTYPE "STATIC" CACHE STRING "" FORCE)
FetchContent_MakeAvailable(openal)
if(TARGET OpenAL AND NOT TARGET OpenAL::OpenAL)
    add_library(OpenAL::OpenAL ALIAS OpenAL)
endif()

# 13. libwebp
message(STATUS "[Standalone] Fetching libwebp...")
FetchContent_Declare(
    libwebp
    GIT_REPOSITORY https://github.com/webmproject/libwebp.git
    GIT_TAG v1.3.2
)
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libwebp)
if(NOT TARGET WebP::WebP)
    add_library(WebP::WebP INTERFACE IMPORTED)
    target_link_libraries(WebP::WebP INTERFACE webp)
    if(TARGET webpdemux)
        target_link_libraries(WebP::WebP INTERFACE webpdemux)
    endif()
endif()

# 14. assimp
message(STATUS "[Standalone] Fetching assimp...")
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.2.5
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECTOR OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

# 15. KTX-Software
message(STATUS "[Standalone] Fetching KTX-Software...")
FetchContent_Declare(
    ktx
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG v4.1.0
)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ktx)
if(TARGET ktx AND NOT TARGET KTX::ktx)
    add_library(KTX::ktx ALIAS ktx)
endif()

# 16. RxCpp
message(STATUS "[Standalone] Fetching RxCpp...")
FetchContent_Declare(
    RxCpp
    GIT_REPOSITORY https://github.com/ReactiveX/RxCpp.git
    GIT_TAG v4.1.1
)
FetchContent_GetProperties(RxCpp)
if(NOT RxCpp_POPULATED)
    FetchContent_Populate(RxCpp)
    add_library(RxCpp INTERFACE)
    target_include_directories(RxCpp INTERFACE ${rxcpp_SOURCE_DIR}/Rx/v2/src/)
endif()

