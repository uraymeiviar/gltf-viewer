include(FetchContent)

#set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG v2.10.3
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  )

FetchContent_GetProperties(pybind11)

if(NOT pybind11_POPULATED)
    FetchContent_Populate(pybind11)
    add_subdirectory(${pybind11_SOURCE_DIR} ${pybind11_BINARY_DIR})
endif()

SET(BUILD_TESTS  OFF)
SET(DOWNLOAD_GTEST  OFF)

FetchContent_Declare(pybind11_json
  GIT_REPOSITORY https://github.com/pybind/pybind11_json.git
  GIT_TAG master
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  )

FetchContent_GetProperties(pybind11_json)
if(NOT pybind11_json_POPULATED)
    FetchContent_Populate(pybind11_json)
    add_subdirectory(${pybind11_json_SOURCE_DIR} ${pybind11_json_BINARY_DIR})
endif()
