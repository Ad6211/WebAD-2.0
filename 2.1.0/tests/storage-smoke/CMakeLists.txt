cmake_minimum_required(VERSION 3.21)
project(WebADStorageSmoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Network Widgets WebEngineWidgets)

add_executable(WebADStorageSmoke storage_smoke.cpp)
target_include_directories(WebADStorageSmoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
target_link_libraries(WebADStorageSmoke PRIVATE Qt6::Network Qt6::Widgets Qt6::WebEngineWidgets)
target_compile_definitions(WebADStorageSmoke PRIVATE
    WEBAD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../.."
)
