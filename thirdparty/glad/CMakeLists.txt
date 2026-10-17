add_library(glad STATIC
    src/glad.c
    include/glad/glad.h
    include/KHR/khrplatform.h
)

target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_definitions(glad PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
