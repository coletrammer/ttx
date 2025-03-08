cmake_minimum_required(VERSION 3.21)

include(meta/cmake/prelude.cmake)

project(
    ttx
    VERSION 0.1.0
    DESCRIPTION "A modern terminal multiplexer"
    HOMEPAGE_URL "https://coletrammer.github.io/ttx"
    LANGUAGES CXX
)

include(meta/cmake/variables.cmake)

# ---- Dependencies ----

# Drop verify interface header sets for our dependencies.
set(want_header_set_verification ${CMAKE_VERIFY_INTERFACE_HEADER_SETS})
set(CMAKE_VERIFY_INTERFACE_HEADER_SETS OFF)

set(ttx_dius_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/dius"
    CACHE STRING "Directory containing the dius library"
)

if(EXISTS "${ttx_dius_DIRECTORY}")
    message(STATUS "Found dius dependency at ${ttx_dius_DIRECTORY}")
    add_subdirectory("${ttx_dius_DIRECTORY}")
else()
    message(STATUS "Looking for dius library via find_package()")
    find_package(dius REQUIRED)
endif()

# Restore verify interface header sets.
set(CMAKE_VERIFY_INTERFACE_HEADER_SETS ${want_header_set_verification})

# ---- Declare ttx library

if(NOT ttx_APP_ONLY)
    add_subdirectory(lib)
else()
    find_package(ttx)
endif()

# ---- Declare executable ----

if(NOT ttx_LIB_ONLY)
    file(
        GLOB_RECURSE sources CONFIGURE_DEPENDS
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "src/*.cpp"
    )
    add_executable(ttx_app ${sources})

    set_property(TARGET ttx_app PROPERTY OUTPUT_NAME ttx)

    target_link_libraries(ttx_app PRIVATE ttx::ttx)
endif()

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
    include(meta/cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT ttx_DEVELOPER_MODE)
    return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
    message(AUTHOR_WARNING "Developer mode is intended for developers of ttx")
endif()

include(meta/cmake/dev-mode.cmake)
