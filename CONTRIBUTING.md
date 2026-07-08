# contributing to cjsonx

first off — thank you for taking the time to contribute. every bug report, fix, and improvement matters.

this document covers the ground rules for contributing to the project. please read it before opening an issue or pull request.

---

## table of contents

- [code of conduct](#code-of-conduct)
- [how to report a bug](#how-to-report-a-bug)
- [how to request a feature](#how-to-request-a-feature)
- [development setup](#development-setup)
- [making a pull request](#making-a-pull-request)
- [coding style](#coding-style)
- [commit message format](#commit-message-format)
- [test requirements](#test-requirements)
- [performance requirements](#performance-requirements)

---

## code of conduct

be respectful. this project follows the [contributor covenant](https://www.contributor-covenant.org/version/2/1/code_of_conduct/).

---

## how to report a bug

open an issue and include:

1. the json input that triggers the bug (as small as possible — a minimal repro)
2. expected behavior vs actual behavior
3. your platform, compiler, and compiler version
4. the output of `git log --oneline -1` so we know which commit you're on

for **security vulnerabilities** (e.g. heap overflows, reads out of bounds), do **not** open a public issue. email the maintainer directly or use [github private vulnerability reporting](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing/privately-reporting-a-security-vulnerability).

---

## how to request a feature

open an issue with a clear description of:

- what you want to do and why
- any relevant prior art (other parsers, rfcs, etc.)
- whether you're willing to implement it yourself

---

## development setup

you need: `cmake >= 3.21`, a c11 compiler (`gcc`, `clang`, or msvc), and `python3` for the amalgamation script.

```bash
git clone https://github.com/tiw302/cjsonx.git
cd cjsonx

# build everything and run tests
./build.sh test

# run the jsontestsuite conformance suite
cd tests && ./run_test_suite.sh
```

optional: run with sanitizers (recommended before submitting a pr):

```bash
cmake -B build_san -S . -DCJSONX_ENABLE_SANITIZERS=ON
cmake --build build_san
cd build_san && ctest
```

---

## making a pull request

1. **fork** the repository and create your branch from `master`
2. **keep pull requests focused** — one logical change per pr
3. **add or update tests** for any behavior you change (see [test requirements](#test-requirements))
4. **run the conformance suite** — all 95 valid + 188 invalid jsontestsuite cases must still pass
5. **run the full test suite** — `./build.sh test` must exit with 100% passing
6. if you touch anything in `include/`, **regenerate the amalgamated header**:
   ```bash
   python3 scripts/amalgamate.py
   ```
   and commit `single_include/cjsonx.h` in the same pr
7. open the pr against the `master` branch

ci will automatically run linux, macos, windows, wasm, sanitizer, and fuzzing checks on your pr.

---

## coding style

this project is written in **pure c11**. consistency with the existing style is more important than personal preference.

### general rules
- **language**: all identifiers, comments, and documentation in english
- **comment case**: all comments are lowercase only — no sentence case
- **short comments** (1–2 lines): `// like this`
- **long comments** (explanatory, algorithmic): `/* like this */`
- **only comment non-obvious things** — do not comment self-explanatory code
- **no trailing whitespace**
- **unix line endings** (lf, not crlf)
- indent with **4 spaces** — no tabs

### naming
- types: `cjsonx_snake_case_t`
- functions: `cjsonx_snake_case()`
- macros: `CJSONX_UPPER_SNAKE_CASE`
- internal / static helpers: `cjsonx_` prefix is still required

### headers
- all modular headers live in `include/`
- every header has an include guard (`#ifndef CJSONX_FOO_H`)
- do not add external dependencies — zero-deps is a hard requirement

### performance
- **no malloc per node** — use the arena allocator
- **no recursion in the hot path** — stage 2 is intentionally iterative
- `CJSONX_LIKELY` / `CJSONX_UNLIKELY` are available for branch hints — use them on error paths

a `.clang-format` config is provided. run it before committing:
```bash
clang-format -i include/*.h src/*.c
```

---

## commit message format

this project uses [conventional commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short description>

[optional body]
```

**types:**
| type | when to use |
|---|---|
| `feat` | new api, feature, or behavior |
| `fix` | bug fix |
| `perf` | performance improvement |
| `refactor` | internal restructure with no behavior change |
| `test` | new or updated tests |
| `docs` | documentation only |
| `ci` | ci/cd changes |
| `build` | build system or amalgamation changes |
| `chore` | maintenance (version bumps, gitignore, etc.) |

**scopes** (optional, use the module name): `stage1`, `stage2`, `arena`, `builder`, `string`, `dom`, `python`, `js`, `wasm`

examples:
```
fix(stage2): handle strtoul overflow for 20+ digit numbers
perf(builder): optimize array append to O(1) using last_child pointer
feat(dom): expose cjsonx_get_len for length-based key lookup
```

---

## test requirements

- tests live in `tests/`
- each new feature or bug fix should have a corresponding test case
- regression tests for bugs should include a comment explaining what the bug was
- tests must pass with sanitizers enabled (`-DCJSONX_ENABLE_SANITIZERS=ON`)
- for new parsing behavior, add a case to `test_conformance.c` or `test_stage2.c`
- for float / number edge cases, add to `test_fastfloat.c` or `test_precision.c`

---

## performance requirements

cjsonx exists specifically for performance. a pr that regresses parsing throughput by more than **5%** on the standard benchmark datasets will not be merged without a very strong justification.

to measure before and after:
```bash
# build with -O3 (default)
./build.sh test

# run benchmark against the standard datasets
./build/bench_compare benchmarks/bench_datasets/twitter.json
./build/bench_compare benchmarks/bench_datasets/canada.json
./build/bench_compare benchmarks/bench_datasets/citm_catalog.json
```

if your change is a performance improvement, include the before/after numbers in your pr description.

---

thank you for contributing to cjsonx.
