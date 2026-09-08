
include(FetchContent)

message(STATUS "Fetching OpenAL")

FetchContent_Declare(openAL
  GIT_REPOSITORY https://github.com/kcat/openal-soft
  GIT_TAG 1.22.0
  )

FetchContent_MakeAvailable(openAL)