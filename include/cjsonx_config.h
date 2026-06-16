// updated 2026-06-13
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_CONFIG_H
#define CJSONX_CONFIG_H

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

// the block size allocated by the memory arena at once
#ifndef CJSONX_ARENA_CHUNK_SIZE
#define CJSONX_ARENA_CHUNK_SIZE 4096
#endif

#endif // cjsonx_config_h
