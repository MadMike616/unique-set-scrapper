cmake_minimum_required(VERSION 3.29)

project(UniqueSetScrapper VERSION 0.3.1 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(UNIQUE_RUNE_CUBE_LOCAL_SDK_DIR "" CACHE PATH
    "Optional PluginSDK include tree for offline development builds")
if(UNIQUE_RUNE_CUBE_LOCAL_SDK_DIR)
    add_library(D2RLPlugin INTERFACE)
    add_library(D2RLPlugin::D2RLPlugin ALIAS D2RLPlugin)
    target_include_directories(D2RLPlugin INTERFACE
        "${UNIQUE_RUNE_CUBE_LOCAL_SDK_DIR}/include")
else()
    include(FetchContent)
    set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(D2RLPLUGIN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        D2RLoaderPluginSDK
        GIT_REPOSITORY https://github.com/D2RLoader/PluginSDK.git
        GIT_TAG 6eb8f8b6192868214706bd6d528c5294f2f551b7
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(D2RLoaderPluginSDK)
endif()

include(FetchContent)
if(NOT TARGET tomlplusplus::tomlplusplus)
    set(TOMLPLUSPLUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(TOMLPLUSPLUS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG v3.4.0 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(tomlplusplus)
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/unique-rune-cube.toml"
    UNIQUE_RUNE_CUBE_DEFAULT_TOML)
configure_file(src/default_config.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/default_config.hpp"
    @ONLY NEWLINE_STYLE LF)

add_library(unique_rune_cube SHARED
    src/plugin.cpp
    src/plugin.rc
)
target_link_libraries(unique_rune_cube PRIVATE D2RLPlugin::D2RLPlugin)
target_link_libraries(unique_rune_cube PRIVATE tomlplusplus::tomlplusplus)
target_include_directories(unique_rune_cube PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(unique_rune_cube PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
)
set_target_properties(unique_rune_cube PROPERTIES
    OUTPUT_NAME "d2rl-unique-rune-cube"
    PREFIX ""
)

if(MSVC)
    target_compile_options(unique_rune_cube PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_options(unique_rune_cube PRIVATE /INCREMENTAL:NO /Brepro)
endif()
