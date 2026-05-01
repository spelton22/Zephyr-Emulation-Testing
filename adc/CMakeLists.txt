cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(adc)

target_sources(app PRIVATE main.c)

if(DEFINED TEST_BUILD AND TEST_BUILD)
    target_compile_definitions(app PRIVATE "main=student_main")
    target_sources(app PRIVATE adc_test_suite.c)
    target_sources(app PRIVATE zephyr_adc_emul_lib.c)
endif()

# # --------------------------------------------------
# # native_sim-only overlay generation
# # --------------------------------------------------
# if(DEFINED BOARD AND BOARD STREQUAL "native_sim")

#   message(STATUS "native_sim build detected - generating devicetree overlay")

#   find_package(Python3 REQUIRED COMPONENTS Interpreter)
  
#   set(INPUT_OVERLAY ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_posix.overlay)
#   set(LED_DATA ${CMAKE_BINARY_DIR}/led_aliases.cmake)
  
#   set(EXTRA_DTC_OVERLAY_FILE
#       ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_sim.overlay
#   )

# endif()

# # --------------------------------------------------
# # Standard Zephyr application setup
# # --------------------------------------------------

# find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})  # defines 'app' target
# project(adc_tests)

# target_sources(app PRIVATE src/main.c)
# target_sources(app PRIVATE testing/bme554_lib.c)
# target_sources(app PRIVATE testing/adc_test.c)

# target_include_directories(app PRIVATE testing)

# target_compile_definitions(app PRIVATE "main=student_main")


# # zephyr_compile_definitions(-Dmain=student_main)