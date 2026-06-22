# =========================================================================
# examples/CMakeLists.txt
# =========================================================================

set(EXAMPLE_FILES 
    dom_access
    float128_precision
    error_handling
    simple_parse
    embedded_noalloc
    builder_api
)

foreach(EXAMPLE IN LISTS EXAMPLE_FILES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE}.c")
        add_executable(example_${EXAMPLE} ${EXAMPLE}.c)
        target_link_libraries(example_${EXAMPLE} PRIVATE cjsonx)
    endif()
endforeach()
