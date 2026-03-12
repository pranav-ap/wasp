# Building

```bash
clear && rm -rf build && cmake -B build -G Ninja && cmake --build build && ctest --test-dir build -L "unit"

clear && cmake --build build && ctest --test-dir build -L "unit"

clear && cmake --build build && ctest --test-dir build -L "unit" --rerun-failed --output-on-failure

ccache -s

sudo ln -s /workspaces/wasp/build/src/wasp /usr/local/bin/wasp

wasp run ./workspace/main.wasp

```

# Linux Setup

```bash
sudo apt update

sudo apt install ccache -y
sudo apt install lld
```