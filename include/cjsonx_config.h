/**
 * @file cjsonx_config.h
 * @brief Compile-time configuration constants
 *
 * @note Architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
#ifndef CJSONX_CONFIG_H
#define CJSONX_CONFIG_H

// configuration constants

// maximum nesting level for arrays and objects to prevent stack overflow
#ifndef CJSONX_MAX_DEPTH
#define CJSONX_MAX_DEPTH 1000
#endif

// initial capacity of the structural tokens tape
#ifndef CJSONX_INITIAL_TAPE_CAP
#define CJSONX_INITIAL_TAPE_CAP 1024
#endif

// initial capacity of elements and fields inside arrays and objects
#ifndef CJSONX_INITIAL_CONTAINER_CAP
#define CJSONX_INITIAL_CONTAINER_CAP 16
#endif

// the block size allocated by the memory arena at once
#ifndef CJSONX_ARENA_CHUNK_SIZE
#define CJSONX_ARENA_CHUNK_SIZE 4096
#endif

#endif // CJSONX_CONFIG_H
