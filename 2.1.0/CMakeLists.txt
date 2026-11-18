cmake_minimum_required(VERSION 3.21)

project(WebAD VERSION 2.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Svg Widgets WebEngineWidgets)

qt_add_executable(WebAD
    src/main.cpp
    src/browser_window.cpp
    src/browser_window.h
    src/navigation_error.h
    src/resources.qrc
)

target_link_libraries(WebAD PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::WebEngineWidgets
)
target_compile_definitions(WebAD PRIVATE WEBAD_VERSION="${PROJECT_VERSION}")

qt_add_executable(WebADUpdater
    updater/main.cpp
)
target_link_libraries(WebADUpdater PRIVATE Qt6::Widgets)
target_compile_definitions(WebADUpdater PRIVATE WEBAD_VERSION="${PROJECT_VERSION}")
set_target_properties(WebADUpdater PROPERTIES
    OUTPUT_NAME WebADUpdater
    WIN32_EXECUTABLE TRUE
)

if(MSVC)
    target_compile_options(WebAD PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WebAD PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_sources(WebAD PRIVATE src/webad.rc)
    target_include_directories(WebAD PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

set_target_properties(WebAD PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
