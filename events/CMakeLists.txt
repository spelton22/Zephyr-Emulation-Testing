cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(events)

target_sources(app PRIVATE main.c)

# if(DEFINED TEST_BUILD AND TEST_BUILD)
    target_compile_definitions(app PRIVATE "main=student_main")
    target_sources(app PRIVATE events_test.c)
    target_sources(app PRIVATE zephyr_events_test_lib.c)
# endif()