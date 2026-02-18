# Building 

```bash

clear && rm -rf build && cmake -B build -G Ninja && cmake --build build && ctest --test-dir build -L "unit"

cmake --build build && ctest --test-dir build -L "unit"

cmake --build build && ctest --test-dir build -L "unit" --rerun-failed --output-on-failure


./build/src/wasp

sudo chown -R vscode:vscode /workspaces/wasp

```

