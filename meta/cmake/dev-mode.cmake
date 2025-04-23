include(CTest)
if(BUILD_TESTING)
    add_subdirectory(lib/test)
    add_subdirectory(test)

    include(meta/cmake/terminal-test.cmake)
endif()

option(ENABLE_COVERAGE "Enable coverage support separate from CTest's" OFF)
if(ENABLE_COVERAGE)
    include(meta/cmake/coverage.cmake)
endif()

option(BUILD_DOCS "Build documentation using Doxygen" OFF)
if(BUILD_DOCS)
    include(meta/cmake/docs.cmake)
endif()

include(meta/cmake/tidy.cmake)
