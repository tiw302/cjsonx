"""
file: file_io.py
description: demonstrates how to read and parse a json file directly
using the c++ bound read_file function.
"""

import cjsonx
import tempfile
import os

def main():
    # create a temporary json file for demonstration
    temp_path = "temp_cjsonx_test.json"
    with open(temp_path, "w") as f:
        f.write('{"engine": "cjsonx", "target": "file_io"}')

    print(f"reading from {temp_path}...")

    try:
        # 1. use cjsonx.read_file to read and parse directly in c++
        doc = cjsonx.read_file(temp_path)

        if doc.is_valid:
            print("parsed successfully!")
            print(f"engine: {doc.root['engine'].get_str()}")
            print(f"target: {doc.root['target'].get_str()}")
        else:
            print(f"failed to parse: {doc.error}")

    except Exception as e:
        print(f"exception occurred: {e}")

    finally:
        # clean up the temp file
        if os.path.exists(temp_path):
            os.remove(temp_path)

if __name__ == "__main__":
    main()
