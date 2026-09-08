cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(lib_name "core_operations")
set(schema_exporter_name "schema_exporter")

project("core_operations")

file (GLOB PYTHON_WRAPPER_FILES "source/*.cpp" "include/GestaltorOperations/*.h")


source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PYTHON_WRAPPER_FILES} )

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

include(thirdparty/pybind11.cmake)

pybind11_add_module(${lib_name} 
	${PYTHON_WRAPPER_FILES}
)

target_include_directories(${lib_name} PUBLIC ${PYBIND11_JSON_INCLUDE_DIR})
target_include_directories(${lib_name} PUBLIC "include")

target_link_libraries(${lib_name} PUBLIC DataStorage)
target_link_libraries(${lib_name} PUBLIC Operations)
target_link_libraries(${lib_name} PUBLIC Utils)


add_executable(${schema_exporter_name} "schema_exporter_source/main.cpp")

target_include_directories(${schema_exporter_name} PUBLIC ${PYBIND11_JSON_INCLUDE_DIR})
target_include_directories(${schema_exporter_name} PUBLIC "include")

target_link_libraries(${schema_exporter_name} PUBLIC DataStorage)
target_link_libraries(${schema_exporter_name} PUBLIC Operations)
target_link_libraries(${schema_exporter_name} PUBLIC Utils)

#OpenSSL
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
include_directories("${OPENSSL_INCLUDE_DIR}")
target_link_libraries(${lib_name} PUBLIC OpenSSL::Crypto)

#USDZ
# set USD parameters
set(usd_install_dir "${usd_repo_SOURCE_DIR}")
set(ufg_install_dir "${usd_from_gltf_repo_SOURCE_DIR}")

if (WIN32)
	file(COPY "${usd_install_dir}/lib/" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
	file(COPY "${usd_install_dir}/bin/" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
  file(COPY "${ufg_install_dir}/bin/usd_from_gltf.exe" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
  file(COPY "${gltf-validator_SOURCE_DIR}/build/bin/gltf_validator.exe" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD_CONFIG_DIR}")
endif()   
