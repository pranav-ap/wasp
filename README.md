# Building 

```bash
cmake -B build -G Ninja
cmake --build build
./build/src/wasp

ctest --test-dir build

cmake --build build && ctest --test-dir build
```

