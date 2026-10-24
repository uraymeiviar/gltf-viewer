set(glm_FOUND TRUE)
set(GLM_FOUND TRUE)
# GLM is vendored in thirdparty/glm (headers only; it used to be a submodule).
set(glm_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/glm" CACHE PATH "" FORCE)
set(glm_INCLUDE_DIRS "${glm_INCLUDE_DIR}")
if(NOT TARGET glm::glm)
    add_library(glm::glm INTERFACE IMPORTED)
    target_include_directories(glm::glm INTERFACE "${glm_INCLUDE_DIR}")
endif()
