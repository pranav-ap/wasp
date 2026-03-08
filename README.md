# Building 

```bash

clear && rm -rf build && cmake -B build -G Ninja && cmake --build build && ctest --test-dir build -L "unit"

clear && rm -rf build && \
cmake -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && \
cmake --build build && \
ctest --test-dir build -L "unit"

clear && cmake --build build && ctest --test-dir build -L "unit"

cmake --build build && ctest --test-dir build -L "unit" --rerun-failed --output-on-failure

ccache -s

sudo ln -s /workspaces/wasp/build/src/wasp /usr/local/bin/wasp

wasp ./samples/main.wasp


```
