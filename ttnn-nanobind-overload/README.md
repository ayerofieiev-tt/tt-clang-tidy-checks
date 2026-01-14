# TTNN Nanobind Unnecessary Overload Check

A custom clang-tidy check that detects unnecessary use of `nanobind_overload_t` when only a single overload is provided in `bind_registered_operation` calls.

## Problem

When binding operations to Python using nanobind, if there's only one overload, you should use `nanobind_arguments_t` instead of `nanobind_overload_t`. The `nanobind_overload_t` type is only necessary when you have multiple overloads.

## Examples

### Bad (unnecessary overload)

```cpp
bind_registered_operation(
    mod,
    ttnn::copy,
    doc,
    ttnn::nanobind_overload_t{  // ❌ Unnecessary - only one overload
        [](const decltype(ttnn::copy)& self, const ttnn::Tensor& input_a, const ttnn::Tensor& input_b) {
            return self(input_a, input_b);
        },
        nb::arg("input_a").noconvert(),
        nb::arg("input_b").noconvert(),
    });
```

### Good (necessary overloads)

```cpp
bind_registered_operation(
    mod,
    ttnn::assign,
    doc,
    ttnn::nanobind_overload_t{  // ✓ Necessary - first overload
        [](const decltype(ttnn::assign)& self, ...) { ... },
        ...
    },
    ttnn::nanobind_overload_t{  // ✓ Necessary - second overload
        [](const decltype(ttnn::assign)& self, ...) { ... },
        ...
    });
```

### Good (no overload needed)

```cpp
bind_registered_operation(
    mod,
    ttnn::fused_rms_minimal,
    doc,
    ttnn::nanobind_arguments_t{  // ✓ Correct - direct arguments
        nb::arg("input_tensor"),
        nb::arg("program_config"),
        ...
    });
```

## Building

The plugin requires:
1. **Clang shared libraries** (from `llvm-17-dev` package)
2. **Clang-tidy development headers** (NOT in packages - need LLVM source)

### Quick Setup

```bash
# 1. Install Clang dev libraries
sudo apt-get install llvm-17-dev

# 2. Get LLVM source (just for headers, shallow clone is fine)
git clone --depth 1 --branch llvmorg-17.0.6 https://github.com/llvm/llvm-project.git /tmp/llvm-project

# 3. Configure and build
cd /path/to/tt-metal/build
cmake .. -DCLANG_TIDY_INCLUDE_DIR=/tmp/llvm-project/clang-tools-extra
cmake --build . --target TtNNNanobindOverloadCheck
```

The plugin will be built as a shared library (`.so` on Linux, `.dylib` on macOS).

## Usage

### Loading the plugin

```bash
clang-tidy -load=build/tools/clang-tidy-plugins/libTtNNNanobindOverloadCheck.so \
           -checks=-*,ttnn-nanobind-unnecessary-overload \
           <source-file>
```

### With compile commands

```bash
clang-tidy -load=build/tools/clang-tidy-plugins/libTtNNNanobindOverloadCheck.so \
           -checks=-*,ttnn-nanobind-unnecessary-overload \
           -p build \
           ttnn/cpp/ttnn/operations/data_movement/copy/copy_nanobind.cpp
```

### Auto-fix

The check provides automatic fixes! Use the `--fix` flag to automatically apply the fixes:

```bash
clang-tidy -load=build/tools/clang-tidy-plugins/libTtNNNanobindOverloadCheck.so \
           -checks=-*,ttnn-nanobind-unnecessary-overload \
           --fix \
           -p build \
           <source-file>
```

The auto-fix will:
1. Replace `nanobind_overload_t` (or `ttnn::nanobind_overload_t`) with `nanobind_arguments_t` (or `ttnn::nanobind_arguments_t`)
2. Remove the lambda expression (first argument) and its trailing comma

**Example transformation:**

Before:
```cpp
ttnn::nanobind_overload_t{
    [](const decltype(ttnn::copy)& self, const ttnn::Tensor& input_a, const ttnn::Tensor& input_b) {
        return self(input_a, input_b);
    },
    nb::arg("input_a").noconvert(),
    nb::arg("input_b").noconvert(),
}
```

After:
```cpp
ttnn::nanobind_arguments_t{
    nb::arg("input_a").noconvert(),
    nb::arg("input_b").noconvert(),
}
```

### Integration with CMake

To use the check during builds, you can configure CMake to load the plugin:

```cmake
set(CMAKE_CXX_CLANG_TIDY 
    "clang-tidy-20;-load=${CMAKE_BINARY_DIR}/tools/clang-tidy-plugins/libTtNNNanobindOverloadCheck.so;-checks=-*,ttnn-nanobind-unnecessary-overload"
)
```

## Check Name

- **Check name**: `ttnn-nanobind-unnecessary-overload`
- **Category**: TTNN-specific checks

## Requirements

- Clang 17 or later
- Clang development libraries (libclang-dev or similar)
- CMake 3.15 or later
