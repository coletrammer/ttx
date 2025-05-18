cmake_minimum_required(VERSION 3.21)

project(ttxLib LANGUAGES CXX)

# ---- Dependencies ----

if(PROJECT_IS_TOP_LEVEL)
    find_package(dius REQUIRED)
endif()

# ---- Declare library ----

file(
    GLOB_RECURSE sources CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "src/*.cpp"
)
file(
    GLOB_RECURSE headers CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "include/*.h"
)

add_library(ttx_ttx ${sources})
add_library(ttx::ttx ALIAS ttx_ttx)

set_target_properties(
    ttx_ttx
    PROPERTIES VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}"
               OUTPUT_NAME ttx
               EXPORT_NAME ttx
)

target_include_directories(ttx_ttx ${warning_guard} PUBLIC "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

target_link_libraries(ttx_ttx PUBLIC dius::dius)

target_sources(
    ttx_ttx
    INTERFACE FILE_SET
              HEADERS
              TYPE
              HEADERS
              BASE_DIRS
              include
              FILES
              ${headers}
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ttx_ttx PRIVATE -fconstexpr-steps=10000000)
endif()
