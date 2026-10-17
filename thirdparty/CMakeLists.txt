# Third-party submodules

# 1. oneTBB
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/oneTBB/CMakeLists.txt")
    set(TBB_TEST OFF CACHE BOOL "Disable TBB tests" FORCE)
    set(TBB_STRICT OFF CACHE BOOL "Disable TBB strict mode" FORCE)
    set(TBB_EXAMPLES OFF CACHE BOOL "Disable TBB examples" FORCE)
    set(TCM_BUILD OFF CACHE BOOL "Disable TCM" FORCE)
    set(TBB_INSTALL OFF CACHE BOOL "Disable TBB install" FORCE)
    set(TBBMALLOC_BUILD OFF CACHE BOOL "Disable tbbmalloc" FORCE)
    set(TBBMALLOC_PROXY_BUILD OFF CACHE BOOL "Disable tbbmalloc proxy" FORCE)
    add_subdirectory(oneTBB)
endif()
