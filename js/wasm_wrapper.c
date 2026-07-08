#define CJSONX_IMPLEMENTATION
#include "../include/cjsonx.h"
#include <stdio.h>
#include <string.h>

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
        current_doc->owned_json = json_copy; // owns copy to prevent use after free
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
            written = snprintf(buf + *offset, max_len - *offset, "%s", cjsonx_bool(val) ? "true" : "false");
            break;
        case CJSONX_NUMBER:
            written = snprintf(buf + *offset, max_len - *offset, "%g", cjsonx_num(val));
            break;
        case CJSONX_STRING:
            written = snprintf(buf + *offset, max_len - *offset, "\"%.*s\"", (int)cjsonx_str_len(val), cjsonx_str(val));
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
                    written = snprintf(buf + *offset, max_len - *offset, "%s  \"%.*s\": ", spaces, (int)cjsonx_str_len(iter.key), cjsonx_str(iter.key));
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

#ifdef __cplusplus
}
#endif
