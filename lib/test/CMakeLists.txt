cmake_minimum_required(VERSION 3.21)

project(libttxTests LANGUAGES CXX)

# ---- Dependencies ----

if(PROJECT_IS_TOP_LEVEL)
    find_package(ttx REQUIRED)
    find_package(dius REQUIRED)
    enable_testing()
endif()

# ---- Unit Tests ----

file(GLOB sources CONFIGURE_DEPENDS src/*.cpp)
add_executable(libttx_test ${sources})

target_link_libraries(libttx_test PRIVATE ttx::ttx dius::dius_test_main)

add_test(NAME libttx_test COMMAND libttx_test)
