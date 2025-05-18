# ---- Dependencies (tic) ----

find_program(TIC tic REQUIRED)
message(STATUS "tic found at ${TIC}.")

# ---- Generate terminfo file ----

set(terminfo_input "${CMAKE_CURRENT_BINARY_DIR}/ttx.terminfo")

add_custom_command(
    OUTPUT "${terminfo_input}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND sh -c "./ttx --terminfo terminfo >${terminfo_input}"
    DEPENDS ttx_app
    COMMENT "Generating ttx terminfo"
    VERBATIM
)

# ---- Compile terminfo file ----

set(terminfo_compiled_output "${CMAKE_CURRENT_BINARY_DIR}/terminfo")
add_custom_command(
    OUTPUT "${terminfo_compiled_output}/x/xterm-ttx" "${terminfo_compiled_output}/t/ttx"
    COMMAND tic -x -o "${terminfo_compiled_output}" "${terminfo_input}"
    DEPENDS "${terminfo_input}"
    COMMENT "Compiling ttx terminfo"
    VERBATIM
)

# ---- Terminfo target ----
add_custom_target(
    ttx_terminfo ALL DEPENDS "${terminfo_input}" "${terminfo_compiled_output}/x/xterm-ttx"
                             "${terminfo_compiled_output}/t/ttx"
)
