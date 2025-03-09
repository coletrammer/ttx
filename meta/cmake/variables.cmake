# ---- Developer mode ----

# Developer mode enables targets and code paths in the CMake scripts that are only relevant for the developer(s) of di
# Targets necessary to build the project must be provided unconditionally, so consumers can trivially build and package
# the project
if(PROJECT_IS_TOP_LEVEL)
    option(ttx_DEVELOPER_MODE "Enable developer mode" OFF)
endif()

# ---- Warning guard ----

# target_include_directories with the SYSTEM modifier will request the compiler to omit warnings from the provided
# paths, if the compiler supports that. This is to provide a user experience similar to find_package when
# add_subdirectory or FetchContent is used to consume this project
set(warning_guard "")
if(NOT PROJECT_IS_TOP_LEVEL)
    option(ttx_INCLUDES_WITH_SYSTEM "Use SYSTEM modifier for ttx's includes, disabling warnings" ON)
    mark_as_advanced(ttx_INCLUDES_WITH_SYSTEM)
    if(ttx_INCLUDES_WITH_SYSTEM)
        set(warning_guard SYSTEM)
    endif()
endif()

# ---- Build config ----

# When including this project as a submodule, the user most like only cares about the ttx library.
set(default_lib_mode OFF)
if(NOT PROJECT_IS_TOP_LEVEL)
    set(set_lib_mode ON)
endif()

option(ttx_APP_ONLY "Only build ttx app" OFF)
option(ttx_LIB_ONLY "Only build ttx lib" ${default_lib_mode})
