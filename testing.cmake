target_sources(app PRIVATE ${CMAKE_SOURCE_DIR}/bme554_lib.c)

if(TEST_FILE)
    target_sources(app PRIVATE ${TEST_FILE})
endif()

target_compile_definitions(app PRIVATE "main=student_main")