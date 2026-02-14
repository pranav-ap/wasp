# Building 

```bash
cmake -B build -G Ninja

cmake --build build

./build/src/wasp

ctest --test-dir build

cmake --build build && ctest --test-dir build
cmake --build build && ctest --test-dir build -L "unit"
cmake --build build && ctest --test-dir build -L "unit" --rerun-failed --output-on-failure


sudo chown -R vscode:vscode /workspaces/wasp

```

