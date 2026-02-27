# ---- Generate shell completions ----

set(bash_output "${CMAKE_CURRENT_BINARY_DIR}/completions/ttx.bash")
set(zsh_output "${CMAKE_CURRENT_BINARY_DIR}/completions/_ttx")

cmake_path(GET bash_output PARENT_PATH bash_directory)
cmake_path(GET zsh_output PARENT_PATH zsh_directory)

add_custom_command(
    OUTPUT "${bash_output}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND sh -c "mkdir -p '${bash_directory}' && ./ttx completions bash >${bash_output}"
    DEPENDS ttx_app
    COMMENT "Generating ttx bash shell completions"
    VERBATIM
)
add_custom_command(
    OUTPUT "${zsh_output}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND sh -c "mkdir -p '${zsh_directory}' && ./ttx completions zsh >${zsh_output}"
    DEPENDS ttx_app
    COMMENT "Generating ttx zsh shell completions"
    VERBATIM
)

# ---- Shell completions targets ----

add_custom_target(ttx_bash_completions ALL DEPENDS "${bash_output}")
add_custom_target(ttx_zsh_completions ALL DEPENDS "${zsh_output}")
add_custom_target(ttx_shell_completions ALL DEPENDS ttx_bash_completions ttx_zsh_completions)
