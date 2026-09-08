include(FetchContent)

FetchContent_Declare(RxCpp
  GIT_REPOSITORY https://github.com/ReactiveX/RxCpp.git
  GIT_TAG v4.1.1
)

FetchContent_GetProperties(RxCpp)
if(NOT RxCpp_POPULATED)
  FetchContent_Populate(RxCpp)
endif()

add_library(RxCpp INTERFACE)
target_include_directories(RxCpp INTERFACE ${rxcpp_SOURCE_DIR}/Rx/v2/src/)
message(STATUS "RxCpp source dir ${rxcpp_SOURCE_DIR}")

