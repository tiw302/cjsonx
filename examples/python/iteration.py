"""
file: iteration.py
description: demonstrates pythonic dictionary-style iteration over object nodes
using .keys(), .values(), and .items().
"""

import cjsonx

def main():
    json_data = '{"id": 101, "name": "alice", "role": "admin", "active": true}'
    doc = cjsonx.parse(json_data)
    
    if not doc.is_valid:
        print("error parsing json")
        return

    root = doc.root

    print("iterating using .keys():")
    for key in root.keys():
        print(f"  key: {key}")

    print("\niterating using .values():")
    for val in root.values():
        print(f"  value type: {val.type}")

    print("\niterating using .items():")
    for key, val in root.items():
        # we can safely check the type before extraction
        if val.type == cjsonx.Type.STRING:
            print(f"  {key} (string) -> {val.get_str()}")
        elif val.type == cjsonx.Type.NUMBER:
            print(f"  {key} (number) -> {val.get_num()}")
        elif val.type == cjsonx.Type.BOOL:
            print(f"  {key} (bool)   -> {val.get_bool()}")

if __name__ == "__main__":
    main()
