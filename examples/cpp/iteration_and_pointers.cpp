/*
 * file: iteration_and_pointers.cpp
 * description: demonstrates modern C++11 range-based for loops and JSON pointers.
 * 
 * how to compile:
 *   1. gcc -c -O3 -D_GNU_SOURCE -I../../include ../../src/cjsonx.c
 *   2. g++ iteration_and_pointers.cpp cjsonx.o -I../../include -std=c++11
 */

#include <iostream>
#include <string>
#include "cjsonx.hpp"

int main() {
    std::string json_str = R"({
        "metadata": {
            "version": 1.0,
            "author": "Alice"
        },
        "tags": ["fast", "simd", "json"],
        "metrics": {
            "speed": 1024.5,
            "memory": 16
        }
    })";

    cjsonx::Document doc = cjsonx::parse(json_str);
    if (!doc.is_valid()) {
        std::cerr << "parse error: " << doc.error_message() << "\n";
        return 1;
    }

    cjsonx::Node root = doc.root();

    std::cout << "--- 1. JSON Pointer Access ---\n";
    // Using RFC 6901 JSON Pointers to fetch nested data directly
    cjsonx::Node author = root.pointer("/metadata/author");
    if (!author.is_null()) {
        std::cout << "Author: " << author.as_string() << "\n";
    }

    std::cout << "\n--- 2. Array Iteration ---\n";
    // Implicit conversion allows receiving Node directly
    for (cjsonx::Node tag : root["tags"]) {
        std::cout << "Tag: " << tag.as_string() << "\n";
    }

    std::cout << "\n--- 3. Object Iteration ---\n";
    // Object iteration yields KV pairs (key and value)
    for (auto kv : root["metrics"]) {
        std::cout << "Metric '" << kv.key() << "' = " << kv.value().as_double() << "\n";
    }

    return 0;
}
