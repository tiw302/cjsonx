# =========================================================================
# tests/CMakeLists.txt
# =========================================================================

# note: test_suite requires a json file argument so it is not added to ctest
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_suite.c")
    add_executable(test_suite test_suite.c)
    target_link_libraries(test_suite PRIVATE cjsonx)
endif()

# standard tests
set(TEST_FILES
    test_stage1
    test_stage2
    test_allocator
    test_builder_remove
    test_pointer
    test_bomb
    test_stringify
    test_builder
    test_fastfloat
    test_utf8
    test_precision
    test_static_buffer
    test_control_char
)

foreach(TEST_NAME IN LISTS TEST_FILES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.c")
        add_executable(${TEST_NAME} ${TEST_NAME}.c)
        target_link_libraries(${TEST_NAME} PRIVATE cjsonx)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endif()
endforeach()

# special test that needs directory arg
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_conformance.c")
    add_executable(test_conformance test_conformance.c)
    target_link_libraries(test_conformance PRIVATE cjsonx)
    add_test(NAME test_conformance COMMAND test_conformance ${CMAKE_CURRENT_SOURCE_DIR}/JSONTestSuite/test_parsing)
endif()
