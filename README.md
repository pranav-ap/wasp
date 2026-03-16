# Wasp

## Building

```bash
clear && rm -rf build && cmake -B build -G Ninja && cmake --build build && ctest --test-dir build -L "unit"

clear && cmake --build build && ctest --test-dir build -L "unit"

clear && cmake --build build && ctest --test-dir build -L "unit" --rerun-failed --output-on-failure

ccache -s

sudo ln -s /workspaces/wasp/build/src/wasp /usr/local/bin/wasp

wasp run ./code/main.wasp

build/src/wasp.exe run ./code/main.wasp

```

## Linux Setup

```bash
ls /usr/bin/lldb-dap* /usr/bin/lldb-vscode*
/usr/bin/lldb-vscode-14
```
