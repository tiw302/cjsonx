#define CJSONX_IMPLEMENTATION
#include <stdio.h>
#include <string.h>

#include "../include/cjsonx.h"

/**
 * @file wasm_wrapper.c
 * @brief WebAssembly entry point and helper functions for Emscripten compilation
 *
 * this wrapper bridges cjsonx parser routines to javascript by exporting clean,
 * simple, null-terminated string parser functions and dynamic memory tools.
 */

#ifdef __cplusplus
extern "C" {
#endif

// global pointer to the currently parsed json document
static cjsonx_doc_t* current_doc = NULL;

/**
 * @brief free memory allocated for the current document
 *
 * releases all nodes and string arena memory associated with current_doc.
 */
void cjsonx_wasm_free() {
    if (current_doc) {
        cjsonx_doc_free(current_doc);
        current_doc = NULL;
    }
}

/**
 * @brief parse a json string in the wasm context
 *
 * @param json a null-terminated json string
 * @return int 1 if parsing succeeded and document is valid, otherwise 0
 */
int cjsonx_wasm_parse(const char* json) {
    cjsonx_wasm_free();
    size_t len = strlen(json);
    char* json_copy = (char*)malloc(len + 1);
    if (!json_copy) return 0;
    memcpy(json_copy, json, len + 1);
    current_doc = cjsonx_parse(json_copy, len);
    if (current_doc) {
        current_doc->owned_json = json_copy;  // owns copy to prevent use after free
        if (current_doc->is_valid) {
            return 1;
        }
    } else {
        free(json_copy);
    }
    return 0;
}

/**
 * @brief retrieve the human-readable error description
 *
 * @return const char* error string describing the parser failure
 */
const char* cjsonx_wasm_get_error() {
    if (!current_doc) return "Memory allocation failed";
    return cjsonx_error_string(current_doc->error);
}

/**
 * @brief retrieve the character offset where the parser failed
 *
 * @return size_t byte offset of the error in the original json string
 */
size_t cjsonx_wasm_get_error_offset() {
    if (!current_doc) return 0;
    return current_doc->error_offset;
}

/**
 * @brief recursively formats a cjsonx value node into a readable indented string
 *
 * @param val current node value handle
 * @param buf destination output buffer
 * @param max_len max size of output buffer
 * @param offset pointer tracking the current write position in buf
 * @param indent spacing level for nested child nodes
 */
static void dump_val(cjsonx_val_t val, char* buf, size_t max_len, size_t* offset, int indent) {
    if (*offset >= max_len - 1) return;

    char spaces[64];
    int sp_len = indent * 2;
    if (sp_len > 63) sp_len = 63;
    memset(spaces, ' ', sp_len);
    spaces[sp_len] = '\0';

    int written = 0;
    cjsonx_type_t type = cjsonx_get_type(val);
    switch (type) {
        case CJSONX_NULL:
            written = snprintf(buf + *offset, max_len - *offset, "null");
            break;
        case CJSONX_BOOL:
            written = snprintf(buf + *offset, max_len - *offset, "%s",
                               cjsonx_bool(val) ? "true" : "false");
            break;
        case CJSONX_NUMBER:
            written = snprintf(buf + *offset, max_len - *offset, "%g", cjsonx_num(val));
            break;
        case CJSONX_STRING:
            written = snprintf(buf + *offset, max_len - *offset, "\"%.*s\"",
                               (int)cjsonx_str_len(val), cjsonx_str(val));
            break;
        case CJSONX_ARRAY:
            written = snprintf(buf + *offset, max_len - *offset, "[\n");
            if (written > 0) *offset += written;
            {
                cjsonx_iter_t iter = cjsonx_iter_init(val);
                size_t count = cjsonx_size(val);
                size_t i = 0;
                while (cjsonx_iter_next(&iter)) {
                    if (*offset >= max_len - 1) return;
                    written = snprintf(buf + *offset, max_len - *offset, "%s  ", spaces);
                    if (written > 0) *offset += written;

                    dump_val(iter.value, buf, max_len, offset, indent + 1);

                    if (i < count - 1) {
                        written = snprintf(buf + *offset, max_len - *offset, ",\n");
                    } else {
                        written = snprintf(buf + *offset, max_len - *offset, "\n");
                    }
                    if (written > 0) *offset += written;
                    i++;
                }
            }
            written = snprintf(buf + *offset, max_len - *offset, "%s]", spaces);
            break;
        case CJSONX_OBJECT:
            written = snprintf(buf + *offset, max_len - *offset, "{\n");
            if (written > 0) *offset += written;
            {
                cjsonx_iter_t iter = cjsonx_iter_init(val);
                size_t count = cjsonx_size(val);
                size_t i = 0;
                while (cjsonx_iter_next(&iter)) {
                    if (*offset >= max_len - 1) return;
                    written = snprintf(buf + *offset, max_len - *offset, "%s  \"%.*s\": ", spaces,
                                       (int)cjsonx_str_len(iter.key), cjsonx_str(iter.key));
                    if (written > 0) *offset += written;

                    dump_val(iter.value, buf, max_len, offset, indent + 1);

                    if (i < count - 1) {
                        written = snprintf(buf + *offset, max_len - *offset, ",\n");
                    } else {
                        written = snprintf(buf + *offset, max_len - *offset, "\n");
                    }
                    if (written > 0) *offset += written;
                    i++;
                }
            }
            written = snprintf(buf + *offset, max_len - *offset, "%s}", spaces);
            break;
    }
    if (written > 0) *offset += written;
}

/**
 * @brief dump AST layout of current document to string
 *
 * @param buf destination output buffer
 * @param max_len maximum character length of the buffer
 * @return int 1 if successful, 0 if no valid document exists
 */
int cjsonx_wasm_dump(char* buf, size_t max_len) {
    if (!current_doc || !current_doc->is_valid) {
        snprintf(buf, max_len, "Invalid document");
        return 0;
    }
    size_t offset = 0;
    dump_val(current_doc->root, buf, max_len, &offset, 0);
    return 1;
}

// get root node index or uint32_max on failure
uint32_t cjsonx_wasm_get_root() {
    if (!current_doc || !current_doc->is_valid) return UINT32_MAX;
    return current_doc->root.node_idx;
}

// get type of node at index
int cjsonx_wasm_get_type(uint32_t idx) {
    if (!current_doc || idx >= current_doc->node_count) return -1;
    cjsonx_val_t val = {current_doc, idx};
    return (int)cjsonx_get_type(val);
}

// get number of elements in array or object
int cjsonx_wasm_get_size(uint32_t idx) {
    if (!current_doc || idx >= current_doc->node_count) return 0;
    cjsonx_val_t val = {current_doc, idx};
    return (int)cjsonx_size(val);
}

// get bool value
int cjsonx_wasm_get_bool(uint32_t idx) {
    if (!current_doc || idx >= current_doc->node_count) return 0;
    cjsonx_val_t val = {current_doc, idx};
    return cjsonx_bool(val) ? 1 : 0;
}

// get number value
double cjsonx_wasm_get_num(uint32_t idx) {
    if (!current_doc || idx >= current_doc->node_count) return 0.0;
    cjsonx_val_t val = {current_doc, idx};
    return cjsonx_num(val);
}

// get string pointer
const char* cjsonx_wasm_get_str(uint32_t idx) {
    if (!current_doc || idx >= current_doc->node_count) return "";
    cjsonx_val_t val = {current_doc, idx};
    const char* s = cjsonx_str(val);
    return s ? s : "";
}

// get string length
int cjsonx_wasm_get_str_len(uint32_t idx) {
    if (!current_doc || idx >= current_doc->node_count) return 0;
    cjsonx_val_t val = {current_doc, idx};
    return (int)cjsonx_str_len(val);
}

// get key of object child
const char* cjsonx_wasm_get_child_key(uint32_t idx, uint32_t child_idx) {
    if (!current_doc || idx >= current_doc->node_count) return "";
    cjsonx_node_t* n = &current_doc->nodes[idx];
    if (cjsonx_node_type(n) != CJSONX_OBJECT) return "";

    size_t len = cjsonx_node_length(n);
    if (child_idx >= len) return "";

    uint32_t curr = n->val.first_child;
    for (uint32_t i = 0; i < child_idx; i++) {
        uint32_t val_idx = current_doc->nodes[curr].next_sibling;
        curr = current_doc->nodes[val_idx].next_sibling;
    }
    cjsonx_node_t* k_node = &current_doc->nodes[curr];
    return k_node->val.str;
}

// get key length of object child
int cjsonx_wasm_get_child_key_len(uint32_t idx, uint32_t child_idx) {
    if (!current_doc || idx >= current_doc->node_count) return 0;
    cjsonx_node_t* n = &current_doc->nodes[idx];
    if (cjsonx_node_type(n) != CJSONX_OBJECT) return 0;

    size_t len = cjsonx_node_length(n);
    if (child_idx >= len) return 0;

    uint32_t curr = n->val.first_child;
    for (uint32_t i = 0; i < child_idx; i++) {
        uint32_t val_idx = current_doc->nodes[curr].next_sibling;
        curr = current_doc->nodes[val_idx].next_sibling;
    }
    cjsonx_node_t* k_node = &current_doc->nodes[curr];
    return (int)cjsonx_node_length(k_node);
}

// get value node index of object child
uint32_t cjsonx_wasm_get_child_val(uint32_t idx, uint32_t child_idx) {
    if (!current_doc || idx >= current_doc->node_count) return UINT32_MAX;
    cjsonx_node_t* n = &current_doc->nodes[idx];
    if (cjsonx_node_type(n) != CJSONX_OBJECT) return UINT32_MAX;

    size_t len = cjsonx_node_length(n);
    if (child_idx >= len) return UINT32_MAX;

    uint32_t curr = n->val.first_child;
    for (uint32_t i = 0; i < child_idx; i++) {
        uint32_t val_idx = current_doc->nodes[curr].next_sibling;
        curr = current_doc->nodes[val_idx].next_sibling;
    }
    return current_doc->nodes[curr].next_sibling;
}

// get array item node index
uint32_t cjsonx_wasm_get_array_item(uint32_t idx, uint32_t item_idx) {
    if (!current_doc || idx >= current_doc->node_count) return UINT32_MAX;
    cjsonx_val_t val = {current_doc, idx};
    cjsonx_val_t res = cjsonx_get_index(val, item_idx);
    if (!res.doc) return UINT32_MAX;
    return res.node_idx;
}

// get object child value index by key
uint32_t cjsonx_wasm_object_get(uint32_t idx, const char* key) {
    if (!current_doc || idx >= current_doc->node_count) return UINT32_MAX;
    cjsonx_val_t val = {current_doc, idx};
    cjsonx_val_t res = cjsonx_get(val, key);
    if (!res.doc) return UINT32_MAX;
    return res.node_idx;
}

// get value index by json pointer path
uint32_t cjsonx_wasm_pointer_get(uint32_t idx, const char* path) {
    if (!current_doc || idx >= current_doc->node_count) return UINT32_MAX;
    cjsonx_val_t val = {current_doc, idx};
    cjsonx_val_t res = cjsonx_pointer_get(val, path);
    if (!res.doc) return UINT32_MAX;
    return res.node_idx;
}

#ifdef __cplusplus
}
#endif
