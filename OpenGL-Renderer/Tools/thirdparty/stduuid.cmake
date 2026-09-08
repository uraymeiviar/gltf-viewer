
include(FetchContent)

message(STATUS "Fetching stduuid")

FetchContent_Declare(stduuid
  GIT_REPOSITORY https://github.com/mariusbancila/stduuid.git
  GIT_TAG master
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  )

FetchContent_GetProperties(stduuid)
if(NOT stduuid_POPULATED)
  FetchContent_Populate(stduuid)
endif()

add_library(stduuid INTERFACE)
target_include_directories(stduuid INTERFACE ${stduuid_SOURCE_DIR}/include)