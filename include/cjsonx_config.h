// updated 2026-07-07
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_CONFIG_H
#define CJSONX_CONFIG_H

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1 // expose posix locales like strtod_l
#endif

//  ██████  ██████  ███    ██ ███████ ██  ██████
// ██      ██    ██ ████   ██ ██      ██ ██
// ██      ██    ██ ██ ██  ██ █████   ██ ██   ███
// ██      ██    ██ ██  ██ ██ ██      ██ ██    ██
//  ██████  ██████  ██   ████ ██      ██  ██████
//
// >>configuration


// ██   ██ ██ ███    ██ ████████ ███████
// ██   ██ ██ ████   ██    ██    ██
// ███████ ██ ██ ██  ██    ██    ███████
// ██   ██ ██ ██  ██ ██    ██         ██
// ██   ██ ██ ██   ████    ██    ███████
//
// >>compiler hints

#if defined(__GNUC__) || defined(__clang__)
// dev note: using __builtin_expect for branch prediction helps the cpu pipeline stay full on hot paths.
#define CJSONX_LIKELY(x) __builtin_expect(!!(x), 1)
#define CJSONX_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define cjsonx_always_inline __attribute__((always_inline)) inline
#define CJSONX_NODISCARD __attribute__((warn_unused_result))
#else
#define CJSONX_LIKELY(x) (x)
#define CJSONX_UNLIKELY(x) (x)
#define cjsonx_always_inline inline
#define CJSONX_NODISCARD
#endif

// export macro for dynamic link libraries
#ifndef CJSONX_API
#define CJSONX_API
#endif

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

/*
 * the block size allocated by the memory arena at once; larger initial chunk means fewer
 * malloc calls during parsing of documents with many strings.
 */
#ifndef CJSONX_ARENA_CHUNK_SIZE
#define CJSONX_ARENA_CHUNK_SIZE 65536
#endif

#endif // cjsonx_config_h
