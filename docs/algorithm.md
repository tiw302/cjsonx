# The cjsonx Algorithm

The architecture of `cjsonx` is designed around extreme performance and memory locality. It achieves parse speeds in excess of 1.0 GB/s by decoupling the parsing process into two distinct phases: **SIMD Structural Scanning (Stage 1)** and **Recursive Descent Parsing (Stage 2)**.

This approach was pioneered by `simdjson` and popularized in C by `yyjson`. `cjsonx` builds upon these foundations by introducing the **Eisel-Lemire float parsing algorithm** into the lexical layer for incredibly fast numerical decoding.

---

## Stage 1: SIMD Structural Scanning

In standard parsers, the CPU spends the vast majority of its time checking every single character one by one (`if (c == ' ') continue;`). This causes massive pipeline stalls and branch mispredictions.

`cjsonx` completely avoids this. Instead, it reads the JSON payload in 32-byte (AVX2) or 16-byte (NEON) chunks.

1. **Bitmask Generation:** SIMD instructions are used to compare the chunk against structural characters (`{`, `}`, `[`, `]`, `:`, `,`, `"`).
2. **String Escaping:** A secondary SIMD pass identifies the start and end of strings, ensuring that characters inside strings (like a colon `:` inside `"time: 12:00"`) are masked out and ignored.
3. **Tape Creation:** The indices of all valid structural characters are compressed into a "Tape" (an array of integers).

By the end of Stage 1, we have a complete map of the JSON structure. Whitespace is mathematically ignored at a speed of several gigabytes per second.

---

## Stage 2: Recursive Descent, Computed Gotos & Arena Allocation

With the Tape constructed, the parser no longer reads the JSON character by character. It jumps directly from one structural character to the next.

1. **Computed Gotos:** On supported compilers (GCC/Clang), the central switch statement is replaced with a dispatch table (`goto *dispatch_table[c]`). This allows the CPU's branch predictor to track the flow of JSON states independently, significantly reducing branch mispredictions and boosting parse speed by 10-15%.
2. **Flat Arena Allocation:** Instead of calling `malloc()` for every object, array, or string, `cjsonx` allocates a single block of memory (an Arena) upfront.
3. **16-byte Nodes:** Every JSON element (whether it's a number, a string reference, or an object) is represented by a highly compressed 16-byte structure (`cjsonx_node_t`).
   - 8 bytes for data (a `double`, a string length, or child indices).
   - 8 bytes for metadata (type, tape index).
4. **Cache Locality:** Because nodes are stored sequentially in the Arena array, traversing the DOM tree is extremely cache-friendly, leading to massive speedups during querying (`cjsonx_get`).

---

## Single-Pass String Parsing & SWAR

When parsing strings, `cjsonx` achieves zero-copy speeds wherever possible. If a string contains escape characters or control characters, it must be validated and potentially re-allocated. 

To maximize throughput, `cjsonx` uses **Single-Pass SIMD & SWAR** techniques:
1. **SIMD (AVX2/NEON/WASM):** It scans 16 to 32 bytes at a time, checking for escapes (`\`), non-ASCII characters, and raw control characters (`< 0x20`) simultaneously in a single CPU instruction.
2. **SWAR (SIMD Within A Register):** On older platforms without vector instructions, it loads 8 bytes into a 64-bit register and uses bitwise arithmetic to validate the entire block concurrently without looping character-by-character.

---

## The Eisel-Lemire Float Engine

Most fast parsers use standard 64-bit IEEE 754 conversions (`strtod` or custom integer math) which can be extremely slow and sometimes inaccurate on complex edge cases.

`cjsonx` leverages the **Eisel-Lemire Algorithm** directly in the parsing pipeline:

1. As the parser reads a number, it accumulates the mantissa and exponent using fast 64-bit integer arithmetic.
2. It then performs a high-precision table lookup (using a precomputed table of powers of 10) and a 128-bit multiplication (`__uint128_t` or emulated on 32-bit platforms).
3. This guarantees that 99.9% of all floating-point numbers are resolved exactly and correctly in a single fast-path operation without any floating-point math overhead.
4. If the number is an extreme edge case that the fast path cannot perfectly resolve, it gracefully falls back to the standard C library (`strtod`) to guarantee 100% correctness.
