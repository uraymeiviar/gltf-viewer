
include(FetchContent)

message(STATUS "Fetching GSL: Guidelines Support Library")

FetchContent_Declare(GSL
  GIT_REPOSITORY https://github.com/microsoft/GSL.git
  GIT_TAG v3.1.0 
  )

FetchContent_MakeAvailable(GSL)
