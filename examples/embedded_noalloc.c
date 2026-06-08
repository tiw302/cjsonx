#include <stdio.h>
#include <string.h>

#include "cjsonx.h"

int main() {
    // 1. a json string
    const char* json = "{\"sensor\": \"temp\", \"value\": 24.5, \"active\": true}";
    size_t len = strlen(json);

    // 2. static buffer on the stack (zero malloc!)
    // 4096 bytes is plenty for this small json.
    // in an embedded system, this could be a global array or rtos pool.
    uint8_t static_buffer[4096];

    // 3. parse without dynamic memory
    cjsonx_doc* doc = cjsonx_parse_with_buffer(json, len, static_buffer, sizeof(static_buffer));

    if (!doc) {
        printf("Failed to initialize document structure (buffer too small).\n");
        return 1;
    }

    if (!doc->is_valid) {
        printf("Parse Error: %s at offset %zu\n", cjsonx_error_string(doc->error), doc->error_offset);
        return 1;
    }

    // 4. access the parsed dom
    cjsonx_val sensor = cjsonx_get(doc->root, "sensor");
    cjsonx_val value = cjsonx_get(doc->root, "value");
    cjsonx_val active = cjsonx_get(doc->root, "active");

    printf("--- Zero-Allocation Parse Successful ---\n");
    printf("Sensor: %.*s\n", (int)cjsonx_str_len(sensor), cjsonx_str(sensor));
    printf("Value:  %.1f\n", cjsonx_num(value));
    printf("Active: %s\n", cjsonx_bool(active) ? "true" : "false");
    printf("Total Nodes: %zu\n", doc->node_count);

    // 5. freeing is safe (and effectively a no-op since it's static)
    cjsonx_doc_free(doc);

    return 0;
}
