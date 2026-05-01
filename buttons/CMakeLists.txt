cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(buttons)

target_sources(app PRIVATE main.c)

# if(DEFINED TEST_BUILD AND TEST_BUILD)
    target_compile_definitions(app PRIVATE "main=student_main")
    target_sources(app PRIVATE button_test_suite.c)
    target_sources(app PRIVATE zephyr_button_test_support.c)
# endif()