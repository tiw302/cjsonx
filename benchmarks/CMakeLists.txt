# =========================================================================
# benchmarks/CMakeLists.txt
# =========================================================================

# note: bench_compare requires third_party libraries (yyjson, cjson, jsmn)
# to be downloaded into benchmarks/third_party/
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/yyjson.c")
    add_executable(bench_compare
        bench_compare.c
        third_party/yyjson.c
        third_party/cJSON.c
    )
    target_include_directories(bench_compare PRIVATE third_party)
    target_link_libraries(bench_compare PRIVATE cjsonx)
endif()
