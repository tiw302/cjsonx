"""
file: python_example.py
description: demonstrates how to use the cjsonx high-performance python bindings
to parse json, traverse the dom, and extract values.
"""

import cjsonx
import json
import time

def main():
    json_data = '{"name": "cjsonx", "version": 1.3, "features": ["simd", "fastfloat", "zero-alloc"], "is_fast": true}'

    print(f"--- parsing json with cjsonx ---")
    start = time.time()

    # 1. parse the json string
    doc = cjsonx.parse(json_data)

    if not doc.is_valid:
        print(f"error parsing json: {doc.error}")
        return

    end = time.time()
    print(f"parsed successfully in {(end - start) * 1000:.4f} ms\n")

    # 2. get the root element (object)
    root = doc.root

    # 3. extract data using pythonic indexing
    name = root["name"].get_str()
    version = root["version"].get_num()
    is_fast = root["is_fast"].get_bool()

    print(f"project: {name} v{version}")
    print(f"is fast? {is_fast}")

    # 4. iterate over arrays
    features_array = root["features"]
    print("features:")
    for i in range(len(features_array)):
        print(f"  - {features_array[i].get_str()}")

if __name__ == "__main__":
    main()
