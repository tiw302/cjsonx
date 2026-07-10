# =========================================================================
# examples/CMakeLists.txt
# =========================================================================

enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(C_EXAMPLES 
    dom_access
    float128_precision
    error_handling
    simple_parse
    embedded_noalloc
    builder_api
)

foreach(EXAMPLE IN LISTS C_EXAMPLES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/c/${EXAMPLE}.c")
        add_executable(example_c_${EXAMPLE} c/${EXAMPLE}.c)
        target_link_libraries(example_c_${EXAMPLE} PRIVATE cjsonx)
    endif()
endforeach()

set(CPP_EXAMPLES 
    cpp_wrapper_example
    iteration_and_pointers
)

foreach(EXAMPLE IN LISTS CPP_EXAMPLES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cpp/${EXAMPLE}.cpp")
        add_executable(example_cpp_${EXAMPLE} cpp/${EXAMPLE}.cpp)
        target_link_libraries(example_cpp_${EXAMPLE} PRIVATE cjsonx)
    endif()
endforeach()
