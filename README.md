# pumpkin

A simple, scalable in-memory key-value store, built step by step.
See [the V1 plan](docs/pumpkin_v1_project_plan.md).

## Build and test

Requirements: CMake ≥ 3.25, Ninja, a C++20 compiler.

```sh
brew install cmake ninja      # macOS
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Use `asan` instead of `debug` to run with AddressSanitizer + UBSan.

## Workflow

Branch from `dev` (`feat/*`, `fix/*`, `chore/*`) → PR into `dev` → promote to `stage` → release to `prod`.
