"""
file: json_pointer.py
description: demonstrates how to query a parsed json document using
rfc 6901 json pointer syntax.
"""

import cjsonx

def main():
    json_data = '{"users": [{"id": 1, "profile": {"name": "bob"}}, {"id": 2, "profile": {"name": "alice"}}]}'
    doc = cjsonx.parse(json_data)
    
    if not doc.is_valid:
        print("error parsing json")
        return

    root = doc.root

    # query the first user's name using a json pointer
    path1 = "/users/0/profile/name"
    try:
        name_node = root.pointer(path1)
        print(f"pointer '{path1}' -> {name_node.get_str()}")
    except Exception as e:
        print(f"error querying pointer: {e}")

    # query the second user's id
    path2 = "/users/1/id"
    try:
        id_node = root.pointer(path2)
        print(f"pointer '{path2}' -> {id_node.get_int()}")
    except Exception as e:
        print(f"error querying pointer: {e}")

    # query a non-existent path
    path3 = "/users/3/profile"
    try:
        root.pointer(path3)
    except Exception as e:
        print(f"pointer '{path3}' intentionally failed: {e}")

if __name__ == "__main__":
    main()
