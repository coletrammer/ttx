cmake_minimum_required(VERSION 3.21)

project(ttxTests LANGUAGES CXX)

# ---- Dependencies ----

if(PROJECT_IS_TOP_LEVEL)
    find_package(ttx REQUIRED)
    find_package(dius REQUIRED)
    enable_testing()
endif()

# ---- Tests ----

file(GLOB sources CONFIGURE_DEPENDS src/*.cpp)
add_executable(ttx_test ${sources})

target_link_libraries(ttx_test PRIVATE ttx::ttx dius::dius_test_main)

add_test(NAME ttx_test COMMAND ttx_test)
