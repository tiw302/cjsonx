# API Reference

The `cjsonx` API is designed to be minimalistic, safe, and intuitive. It uses opaque 16-byte handles (`cjsonx_val_t`) to interact with the underlying DOM arena, ensuring type safety and preventing use-after-free errors.

---

## Core Types

### `cjsonx_doc_t`

The main document container. It holds the flat DOM arena, the structural tape, and the original JSON string.

```c
typedef struct {
    bool is_valid;             // True if parsing succeeded
    cjsonx_error_t error;      // Error code if parsing failed
    size_t error_offset;       // Byte offset of the syntax error
    cjsonx_val_t root;         // The root value handle
    bool is_static;            // True if backed by user-provided static buffer
    // ... internal fields ...
} cjsonx_doc_t;
```

### `cjsonx_val_t`

A lightweight 16-byte handle representing a JSON node (Object, Array, String, Number, Boolean, or Null). Passed by value.

```c
typedef struct {
    cjsonx_doc_t* doc;
    uint32_t node_idx;
} cjsonx_val_t;
```

### `cjsonx_type_t`

Enumeration of possible JSON types.

```c
typedef enum {
    CJSONX_NULL = 0,
    CJSONX_BOOL,
    CJSONX_NUMBER,
    CJSONX_STRING,
    CJSONX_ARRAY,
    CJSONX_OBJECT
} cjsonx_type_t;
```

---

## Parsing & Memory Management

### `cjsonx_parse`

```c
cjsonx_doc_t* cjsonx_parse(const char* json, size_t length);
```

Parses a JSON string.

- **`json`**: The UTF-8 JSON payload. Does not need to be null-terminated.
- **`length`**: The byte length of the payload.
- **Returns**: A dynamically allocated document. Returns `NULL` if `malloc` fails internally. You must check `doc->is_valid` to ensure syntax correctness.

### `cjsonx_parse_with_buffer` (Zero-Allocation Mode)

```c
cjsonx_doc_t* cjsonx_parse_with_buffer(const char* json, size_t length, void* buffer, size_t buffer_size);
```

Parses a JSON string using entirely user-provided memory. This avoids `malloc` entirely, making it perfect for embedded systems or RTOS environments.

- **`buffer`**: A static byte array or memory pool.
- **`buffer_size`**: The size of the buffer. If parsing exhausts this size, the parser fails with `CJSONX_ERROR_OOM`.
- **Read-Only Result:** The returned document is marked `is_static = true`. The Builder API (`cjsonx_object_set`, `cjsonx_array_push`, etc.) will return `false` on static documents because the node array cannot grow. `cjsonx_doc_free()` on a static document is a safe no-op.

---

## Owned-Copy Parsing

Use these variants when you can't guarantee the lifetime of the input buffer, or when you want to free/modify the buffer immediately after the call. The document makes an internal copy and owns it — the copy is freed automatically when you call `cjsonx_doc_free()`.

### `cjsonx_parse_copy`

```c
cjsonx_doc_t* cjsonx_parse_copy(const char* json, size_t length);
cjsonx_doc_t* cjsonx_parse_copy_ex(const char* json, size_t length, cjsonx_allocator_t* alloc);
cjsonx_doc_t* cjsonx_parse_copy_cstr(const char* json); // convenience for null-terminated strings
```

Copies the input buffer before parsing. Ideal for cases like:
- Parsing a buffer received from a network socket that will be reused.
- Using `cjsonx` from bindings where the input string lifetime is short (e.g. WASM JS bridge, Python bindings).
- Any situation where you don't control the input buffer's lifetime.

### `cjsonx_doc_free`

```c
void cjsonx_doc_free(cjsonx_doc_t* doc);
```

Frees the entire document and its internal arenas. Any `cjsonx_val_t` handles associated with this document become invalid.

---

## DOM Access

### `cjsonx_get_type`

```c
cjsonx_type_t cjsonx_get_type(cjsonx_val_t val);
```

Returns the type of the value. If the handle is invalid or null, returns `CJSONX_NULL`.

### `cjsonx_get`

```c
cjsonx_val_t cjsonx_get(cjsonx_val_t obj, const char* key);
```

Retrieves a child value from a JSON Object by its exact null-terminated string key. O(N) linear scan.

- Returns a null handle if the key is not found, or if `obj` is not an Object.

### `cjsonx_get_len`

```c
cjsonx_val_t cjsonx_get_len(cjsonx_val_t obj, const char* key, size_t key_len);
```

Same as `cjsonx_get` but accepts a key with an explicit byte length. Useful when the key is a substring or is **not null-terminated** (e.g., when the key pointer comes directly from `cjsonx_str()` on another node).

### `cjsonx_get_index`

```c
cjsonx_val_t cjsonx_get_index(cjsonx_val_t arr, size_t index);
```

Retrieves a child value from a JSON Array by its zero-based index. O(N) sibling walk.

- Returns a null handle if the index is out of bounds, or if `arr` is not an Array.

---

## Value Retrieval

### `cjsonx_str`

```c
const char* cjsonx_str(cjsonx_val_t val);
```

Retrieves the string pointer.

**Warning:** `cjsonx` is a zero-copy parser by default. When using `cjsonx_parse()`, the returned string points directly into the original JSON payload and is **not null-terminated** — always use `cjsonx_str_len()` to bound the read. When using `cjsonx_parse_copy()`, the document owns the buffer, but strings are still **not guaranteed to be null-terminated**.

### `cjsonx_str_len`

```c
size_t cjsonx_str_len(cjsonx_val_t val);
```

Returns the exact byte length of the string.

### `cjsonx_num`

```c
double cjsonx_num(cjsonx_val_t val);
```

Retrieves the 64-bit double precision representation of a JSON number.

### `cjsonx_int`

```c
int64_t cjsonx_int(cjsonx_val_t val);
```

Retrieves the numerical value as a strict 64-bit integer. Useful for parsing large IDs.

### `cjsonx_bool`

```c
bool cjsonx_bool(cjsonx_val_t val);
```

Returns `true` or `false` for boolean nodes.

### `cjsonx_size`

```c
size_t cjsonx_size(cjsonx_val_t val);
```

Returns the number of elements inside an Array or the number of key-value pairs inside an Object.

### `cjsonx_is_null`

```c
bool cjsonx_is_null(cjsonx_val_t val);
```

Returns `true` if the value is explicitly a JSON `null`, or if the handle itself is invalid/empty.

---

## Iteration

### `cjsonx_iter_t`

A lightweight iterator struct for walking through Arrays and Objects.

```c
typedef struct {
    cjsonx_val_t key;   // Populated if iterating an Object
    cjsonx_val_t value; // The child value (Array element or Object value)
    bool is_object;
    bool valid;
    // ... internal fields ...
} cjsonx_iter_t;
```

### `cjsonx_iter_init` & `cjsonx_iter_next`

```c
cjsonx_iter_t cjsonx_iter_init(cjsonx_val_t val);
bool cjsonx_iter_next(cjsonx_iter_t* iter);
```

Creates an iterator for an Array or Object and advances it.

**Example: Iterating an Array**

```c
cjsonx_iter_t it = cjsonx_iter_init(my_array);
while (cjsonx_iter_next(&it)) {
    double n = cjsonx_num(it.value);
}
```

**Example: Iterating an Object**

```c
cjsonx_iter_t it = cjsonx_iter_init(my_object);
while (cjsonx_iter_next(&it)) {
    printf("Key: %.*s\n", (int)cjsonx_str_len(it.key), cjsonx_str(it.key));
}
```

---

## Error Handling

### `cjsonx_error_string`

```c
const char* cjsonx_error_string(cjsonx_error_t err);
```

Converts a `cjsonx_error_t` enum into a human-readable string (e.g., `"Invalid structural character"`, `"Unterminated string"`).

---

## Builder API & Stringification

`cjsonx` allows you to create and mutate JSON documents dynamically, and convert them back into JSON strings.

### Creating Values

| Function | Signature | Description |
|---|---|---|
| `cjsonx_create_null` | `cjsonx_val_t cjsonx_create_null(cjsonx_doc_t* doc)` | Creates a `null` node in the document. |
| `cjsonx_create_bool` | `cjsonx_val_t cjsonx_create_bool(cjsonx_doc_t* doc, bool val)` | Creates a boolean node. |
| `cjsonx_create_number` | `cjsonx_val_t cjsonx_create_number(cjsonx_doc_t* doc, double val)` | Creates a number node. |
| `cjsonx_create_string` | `cjsonx_val_t cjsonx_create_string(cjsonx_doc_t* doc, const char* str)` | Creates a string node. The string is copied into the document arena. |
| `cjsonx_create_object` | `cjsonx_val_t cjsonx_create_object(cjsonx_doc_t* doc)` | Creates an empty JSON Object. |
| `cjsonx_create_array` | `cjsonx_val_t cjsonx_create_array(cjsonx_doc_t* doc)` | Creates an empty JSON Array. |

### Mutating Structures

| Function | Signature | Description |
|---|---|---|
| `cjsonx_object_set` | `bool cjsonx_object_set(cjsonx_val_t obj, const char* key, cjsonx_val_t val)` | Inserts or appends a key-value pair into an Object. |
| `cjsonx_array_push` | `bool cjsonx_array_push(cjsonx_val_t arr, cjsonx_val_t val)` | Appends a value to the end of an Array. |
| `cjsonx_object_remove` | `bool cjsonx_object_remove(cjsonx_val_t obj, const char* key)` | Removes a key-value pair from an Object. |
| `cjsonx_array_remove` | `bool cjsonx_array_remove(cjsonx_val_t arr, size_t index)` | Removes a value at the given index from an Array. |

### Stringification

| Function | Signature | Description |
|---|---|---|
| `cjsonx_stringify` | `char* cjsonx_stringify(cjsonx_doc_t* doc)` | Converts the entire document into a minified, null-terminated JSON string. The returned string is `malloc`'d and must be freed by the caller. |
| `cjsonx_stringify_format` | `char* cjsonx_stringify_format(cjsonx_doc_t* doc, int indent)` | Same as `stringify`, but pretty-prints the JSON with `indent` spaces per level. |

---

## JSON Pointer (RFC 6901)

You can query deeply nested JSON structures using a single path string.

| Function | Signature | Description |
|---|---|---|
| `cjsonx_pointer_get` | `cjsonx_val_t cjsonx_pointer_get(cjsonx_val_t root, const char* path)` | Retrieves a node using a JSON Pointer path (e.g. `"/user/profile/age"`). Returns a `null` handle if the path is invalid or not found. Properly unescapes `~1` to `/` and `~0` to `~`. |

---

## Custom Allocator Injection

For strict memory environments (e.g., game engines, custom OS kernels), you can override the standard `<stdlib.h>` allocation functions using `cjsonx_parse_ex`.

### `cjsonx_allocator_t`

```c
typedef struct {
    void* (*malloc_fn)(size_t size, void* user_data);
    void* (*realloc_fn)(void* ptr, size_t size, void* user_data);
    void  (*free_fn)(void* ptr, void* user_data);
    void* user_data;
} cjsonx_allocator_t;
```

| Function | Signature | Description |
|---|---|---|
| `cjsonx_parse_ex` | `cjsonx_doc_t* cjsonx_parse_ex(const char* json, size_t len, cjsonx_allocator_t* alloc)` | Parses the JSON document using the provided allocator functions. If `alloc` is `NULL`, it falls back to standard `malloc` and `free`. |

---

## Cloning and Patching

### `cjsonx_clone_val`

```c
cjsonx_val_t cjsonx_clone_val(cjsonx_doc_t* dest_doc, cjsonx_val_t src_val);
```

Recursively clones a value node and all of its children into the destination document's arena. Returns the cloned node handle.

### `cjsonx_merge_patch`

```c
cjsonx_val_t cjsonx_merge_patch(cjsonx_val_t target, cjsonx_val_t patch);
```

Applies a JSON Merge Patch (RFC 7396) to a target node. Returns the modified node handle.

---

## File I/O Utilities

### `cjsonx_read_file` & `cjsonx_read_file_ex`

```c
cjsonx_doc_t* cjsonx_read_file(const char* path);
cjsonx_doc_t* cjsonx_read_file_ex(const char* path, cjsonx_allocator_t* alloc);
```

Reads the JSON file from the filesystem path and parses it. Returns the parsed document handle, or `NULL` if opening/reading fails.

### `cjsonx_write_file` & `cjsonx_write_file_format`

```c
bool cjsonx_write_file(const char* path, cjsonx_doc_t* doc);
bool cjsonx_write_file_format(const char* path, cjsonx_doc_t* doc, int indent);
```

Serializes the document into a JSON file at the specified path. Returns `true` on success, or `false` on file writing failure.

---

## Limits & Warnings

- **16MB Node Size Limit:** The 16-byte DOM node packs its type and length field into a single `uint32_t` (8 bits for type, 24 bits for length). This limits the maximum length of any single string or serialized container to `16,777,215` bytes. Inputs exceeding this limit will fail with `CJSONX_ERROR_TOO_LARGE` — no silent truncation occurs.
- **O(N) Array Push Complexity:** Pushing an element to an array via `cjsonx_array_push` traverses the list of siblings to find the end. Building large arrays sequentially through push operations results in O(N^2) complexity.
- **Nesting Depth Guard:** `cjsonx_stringify` and `cjsonx_stringify_format` enforce a nesting depth limit of 1000 (`CJSONX_MAX_DEPTH`) to prevent stack overflow on extremely nested inputs. This value is compile-time configurable.
- **Static Buffer Read-Only:** Documents created with `cjsonx_parse_with_buffer()` are read-only. The Builder API will return `false` on any mutation attempt. `cjsonx_doc_free()` is a safe no-op for static documents.
- **Zero-Copy Lifetime:** Documents created with `cjsonx_parse()` hold pointers into the original input buffer. The buffer must stay alive for the entire lifetime of the document. Use `cjsonx_parse_copy()` if you need to free the input buffer early.
