# ---- Dependencies ----

set(doxygen_awesome_SOURCE_DIR
    "$ENV{TTX_DOXYGEN_AWESOME_DIR}"
    CACHE STRING "Doxygen awesome CSS source directory"
)

find_package(Doxygen OPTIONAL_COMPONENTS dot)

# ---- Declare documentation target ----

set(DOXYGEN_OUTPUT_DIRECTORY
    "${PROJECT_BINARY_DIR}/docs"
    CACHE PATH "Path for the generated Doxygen documentation"
)

set(working_dir "${PROJECT_BINARY_DIR}/docs")

configure_file("docs/Doxyfile.in" "${working_dir}/Doxyfile" @ONLY)

doxygen_add_docs(docs ${CMAKE_SOURCE_DIR}/include CONFIG_FILE "${working_dir}/Doxyfile" COMMENT "Generate Doxygen docs")

add_custom_target(open_docs COMMAND /usr/bin/env xdg-open "file://${DOXYGEN_OUTPUT_DIRECTORY}/html/index.html")
