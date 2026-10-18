include(FetchContent)

message(STATUS "Fetching gltf-schema")

FetchContent_Declare(gltf-schema
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/gltf-schema"
)

FetchContent_MakeAvailable(gltf-schema)

if(NOT TARGET gltf-schema)
  add_library(gltf-schema INTERFACE)
  target_include_directories(gltf-schema INTERFACE ${gltf-schema_BINARY_DIR}/include/)
endif()