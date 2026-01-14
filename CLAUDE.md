# Claude Notes

## Project Overview

This repo contains custom clang-tidy checks for Tenstorrent's TTNN codebase.

## Building

```bash
mkdir build && cd build
cmake .. -DCLANG_VERSION=17  # or 18
make -j$(nproc)
```

The CMake configuration automatically downloads required clang-tidy headers from LLVM GitHub.

## Running the Check

```bash
# With compile_commands.json from tt-metal
clang-tidy-17 -load /path/to/TtNNNanobindOverloadCheck.so \
  -checks='-*,ttnn-nanobind-unnecessary-overload' \
  -p /path/to/tt-metal/build \
  <source-file>
```

## Check: `ttnn-nanobind-unnecessary-overload`

**Purpose:** Detects when `nanobind_overload_t` is used with only a single overload, where `nanobind_arguments_t` should be used instead.

**When it triggers:**
- `bind_registered_operation()` call has exactly ONE `nanobind_overload_t` argument

**When it does NOT trigger:**
- Multiple `nanobind_overload_t` arguments (legitimate use case)
- Zero `nanobind_overload_t` arguments

## CI

GitHub Actions builds and tests on both Clang 17 and 18. Pushing a tag like `v1.x.x` automatically creates a release with pre-built binaries.

## Related

- Used with: https://github.com/tenstorrent/tt-metal
- nanobind bindings in: `ttnn/cpp/ttnn/operations/**/nanobind.cpp`
