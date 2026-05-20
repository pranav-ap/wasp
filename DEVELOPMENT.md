# Wasp

## Building

```bash
clear && cmake --build build
```

## Building and Testing

**Clean Build and Test**

Wipes the existing build directory, generates Ninja build files, compiles, and runs all unit tests.

```bash
clear && rm -rf build && cmake -B build -G Ninja && cmake --build build && ctest --test-dir build -L "unit"
```

**Incremental Build and Test**

Compiles only the changed files and runs all unit tests.

```bash
clear && cmake --build build --parallel 2 && ctest --test-dir build -L "unit"
```

**Debug Failed Tests**

Builds and re-runs only the tests that previously failed, outputting the failure logs directly to the console.

```bash
clear && cmake --build build && ctest --test-dir build -L "unit" --rerun-failed --output-on-failure
```

**Check Compiler Cache**

View `ccache` statistics to ensure your builds are hitting the cache.

```bash
ccache -s
```


## Installation and Execution

**Create a Global Symlink**
Allows you to run the `wasp` command globally from any directory.
```bash
sudo ln -s /workspaces/wasp/build/src/wasp /usr/local/bin/wasp
```

**Run a Wasp Script**
```bash
# If symlinked globally:
wasp run ./code/main.wasp

# If running directly from the build directory:
build/src/wasp.exe run ./code/main.wasp
```

## Linux Debugger Setup

To configure your IDE debugger (like `launch.json` in VS Code), locate your installed LLDB adapter path:

```bash
ls /usr/bin/lldb-dap* /usr/bin/lldb-vscode*
```
*(Example output: `/usr/bin/lldb-vscode-14`)*

---

