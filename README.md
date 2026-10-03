# pumpkin

A simple, scalable in-memory multicore key-value store

## Build and test

Requirements: CMake ≥ 3.25, Ninja, a C++20 compiler.

```sh
brew install cmake ninja      # macOS
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Use `asan` instead of `debug` to run with AddressSanitizer + UBSan.
