# Wasp - Programming Language Compiler

## Build Commands

```bash
# Clean build + test (Ninja generator)
rm -rf build && cmake -B build -G Ninja && cmake --build build && ctest --test-dir build -L "unit"

# Incremental build + test
cmake --build build --parallel 2 && ctest --test-dir build -L "unit"

# Run only failed tests
ctest --test-dir build -L "unit" --rerun-failed --output-on-failure
```

## Test Suites

Test binaries are placed in `build/tests/`: `lexer_tests`, `parser_tests`, `semantics_tests`, `compiler_tests`.

Run a specific test binary directly:
```bash
./build/tests/lexer_tests
./build/tests/parser_tests
./build/tests/compiler_tests
```

## Architecture

The Wasp compiler is a pipeline:
1. **Captain** (`libs/captain/`) - orchestrator that drives the build process
2. **Lexer** → **Parser** → **Semantic Analyzer** → **Compiler** → **VM**
3. **Workspace** (`libs/workspace/`) - manages modules and dependencies
4. Entry point: `src/main.cpp`, binary: `build/src/wasp`

## Code Style

- C++23 with Allman braces (see `.clang-format`)
- Format before committing: `clang-format -i <files>`
- Strict includes (`UnusedIncludes: Strict` in `.clangd`)

## Library Dependencies

Libraries are in `libs/` and `include/`. External deps fetched via CMake FetchContent:
- **CLI11** - header-only, in `include/`
- **cpptrace** - stack traces (v0.7.5)
- **googletest** - testing (v1.17.0)

## Wasp Language

See `specs.md` for the Wasp language specification. Example entry point:
```bash
wasp run ./code/main.wasp
# or before symlinking:
build/src/wasp.exe run ./code/main.wasp
```
