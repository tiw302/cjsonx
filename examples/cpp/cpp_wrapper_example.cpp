/*
 * file: cpp_wrapper_example.cpp
 * description: demonstrates how to use the c++ object-oriented wrapper for cjsonx.
 * 
 * how to compile:
 *   cjsonx implementation must be compiled as C code, not C++.
 *   1. gcc -c -O3 -D_GNU_SOURCE -I../../include ../../src/cjsonx.c
 *   2. g++ cpp_wrapper_example.cpp cjsonx.o -I../../include
 */

#include <iostream>
#include <string>
#include "cjsonx.hpp" // assuming a cjsonx.hpp exists for c++ wrapper

int main() {
    std::string json_str = R"({
        "status": "success",
        "data": {
            "id": 404,
            "message": "not found"
        }
    })";

    std::cout << "--- cjsonx c++ wrapper example ---\n";

    // parse json utilizing raii semantics
    cjsonx::Document doc = cjsonx::parse(json_str);

    if (!doc.is_valid()) {
        std::cerr << "parse error: " << doc.error_message() << "\n";
        return 1;
    }

    // fluent api style access
    cjsonx::Node root = doc.root();
    
    std::string status(root["status"].as_string());
    int id = root["data"]["id"].as_int();
    std::string msg(root["data"]["message"].as_string());

    std::cout << "status: " << status << "\n";
    std::cout << "error " << id << ": " << msg << "\n";

    return 0;
}
