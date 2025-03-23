# ---- Terminal Test Commands ----

add_custom_target(
    generate-terminal-test
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMAND /bin/sh -c
            "'${CMAKE_SOURCE_DIR}/meta/scripts/generate-terminal-test.sh' '$$TTX_GENERATE_TERMINAL_TEST_ARGS'"
    USES_TERMINAL
)

add_custom_target(
    run-terminal-test
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMAND /bin/sh -c "'${CMAKE_SOURCE_DIR}/meta/scripts/run-terminal-test.sh' '$$TTX_RUN_TERMINAL_TEST_ARGS'"
    USES_TERMINAL
)

# ---- Terminal Tests ----

file(GLOB terminal_tests CONFIGURE_DEPENDS lib/test/cases/*.input.ansi)

foreach(terminal_test ${terminal_tests})
    get_filename_component(test_name "${terminal_test}" NAME_WE)
    add_test(NAME "terminal:${test_name}" COMMAND "${PROJECT_SOURCE_DIR}/meta/scripts/run-terminal-test.sh"
                                                  "${test_name}"
    )
endforeach()
