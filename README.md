# Pumpkin

A simple, scalable in-memory key-value store in C++20, built step by step towards a multicore,
persistent design.

> **Status: early development.** Pumpkin currently runs as a single-threaded server with an
> in-memory store. Multicore execution and persistence are planned (see [Roadmap](#roadmap)).

## Features

- **Commands:** `PING`, `SET key value [key value ...]`, `GET key`, `DEL key`
- **Text protocol** for debugging: one command per line, usable with `nc`
- **Binary protocol (PBWP v1)** codec: length-prefixed frames, binary-safe keys and values,
  request ids, pipelining (server integration in progress)
- **Non-blocking networking** with `poll()`, pipelining, backpressure and half-close handling
- **Structured logging** (spdlog) with per-module levels; values are never logged
- **Tested** with GoogleTest, AddressSanitizer + UBSan, and `-Werror` CI on Linux (GCC) and
  macOS (Clang)

## Quick start

Requirements: CMake ≥ 3.25, Ninja, a C++20 compiler.

```sh
brew install cmake ninja      # macOS
cmake --preset debug
cmake --build --preset debug
./build/debug/apps/pumpkin-server          # listens on 127.0.0.1:7379
```

In another terminal:

```sh
nc 127.0.0.1 7379
```

```
PING
PONG
SET name Alice
OK
GET name
VALUE Alice
DEL name
INT 1
GET name
NIL
```

Stop the server with Ctrl+C. To use another port: `./build/debug/apps/pumpkin-server 8000`.

## Development

```sh
cmake --build --preset debug && ctest --preset debug                        # build and run all tests
cmake --preset asan && cmake --build --preset asan && ctest --preset asan   # with sanitizers
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')             # formatting check
```

Branches: `feat/*`, `fix/*`, `refactor/*` and `chore/*` are branched from `dev` and merged back
through pull requests; `dev` → `stage` → `prod`.

## Project layout

```
apps/pumpkin-server/        the server executable (wiring only)
src/common/                 pump-cmn-log (logging), pump-cmn-stat (status codes)
src/core/                   pump-core-eng (the key-value store)
src/middlewares/            pump-mid-pwp (text protocol), pump-mid-pbwp (binary protocol),
                            pump-mid-cde (command dispatch)
src/services/               pump-srv-net (TCP server and sockets)
tests/                      unit and integration tests, mirroring src/
```

Layers depend only on the layers below them: common → core → middlewares → services → apps.

## License

Licensed under the [Apache License, Version 2.0](LICENSE).
