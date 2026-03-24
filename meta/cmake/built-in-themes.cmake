# ---- Dependencies ----

set(ENV_TTX_ITERM2_COLOR_SCHEMES_DIR $ENV{TTX_ITERM2_COLOR_SCHEMES_DIR})

if(ENV_TTX_ITERM2_COLOR_SCHEMES_DIR)
    set(ttx_iterm2_color_schemes_SOURCE_DIR
        "$ENV{TTX_ITERM2_COLOR_SCHEMES_DIR}"
        CACHE STRING "iTerm2 color schemes source directory"
    )
else()
    set(ttx_iterm2_color_schemes_SOURCE_DIR
        "${CMAKE_SOURCE_DIR}/iTerm2-Color-Schemes"
        CACHE STRING "iTerm2 color schemes source directory"
    )
endif()

# ---- Generate themes ----

file(GLOB theme_files CONFIGURE_DEPENDS "${ttx_iterm2_color_schemes_SOURCE_DIR}/kitty/*")

set(output_file "${CMAKE_CURRENT_BINARY_DIR}/built_in_themes.cpp")
set(script_file "${CMAKE_SOURCE_DIR}/meta/scripts/generate-built-in-themes.py")
add_custom_command(
    OUTPUT "${output_file}"
    COMMAND python "${script_file}" -i "${ttx_iterm2_color_schemes_SOURCE_DIR}/kitty" -o "${output_file}"
    DEPENDS ${theme_files} "${script_file}"
)

# ---- Add compile options ----

target_compile_definitions(ttx_ttx_app_lib PUBLIC TTX_BUILT_IN_THEMES)

target_sources(ttx_ttx_app_lib PRIVATE "${output_file}")
