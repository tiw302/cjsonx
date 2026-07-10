"""
file: error_handling.py
description: demonstrates how to catch syntax errors and use doc.error_offset
to locate the exact position of the error in python.
"""

import cjsonx

def main():
    # a malformed json string (missing closing quote)
    json_data = '{\n  "name": "cjsonx,\n  "fast": true\n}'

    print("attempting to parse malformed json...\n")

    # 1. parse the json string
    doc = cjsonx.parse(json_data)

    # 2. check if valid
    if not doc.is_valid:
        print("error detected!")
        print(f"message: {doc.error}")
        print(f"offset:  {doc.error_offset}\n")

        # 3. visually point to the error
        start = max(0, doc.error_offset - 15)
        snippet = json_data[start:doc.error_offset + 10].replace('\n', ' ')
        print("json snippet:")
        print(f"{snippet}...")
        spaces = doc.error_offset - start
        print(" " * spaces + "^")
    else:
        print("parsed successfully (this should not happen for this input)")

if __name__ == "__main__":
    main()
