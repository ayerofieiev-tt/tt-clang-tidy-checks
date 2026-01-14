# Check: `ttnn-return-value-type-alias`

## Purpose

Removes redundant return value type aliases and replaces all their usages with direct types.

## Background

The `*_device_operation_types.hpp` files sometimes contain type aliases like:
```cpp
using spec_return_value_t = TensorSpec;
using tensor_return_value_t = Tensor;
```

These are **redundant** because:
1. The device operation struct should define these types directly
2. All usages can simply use `Tensor` or `TensorSpec` directly

## What It Does

### 1. Removes redundant aliases from types files

```cpp
// BEFORE: *_device_operation_types.hpp
using spec_return_value_t = TensorSpec;      // Removed
using tensor_return_value_t = Tensor;         // Removed
```

### 2. Replaces all namespace-qualified usages everywhere

```cpp
// BEFORE: Any file using the namespace alias
using spec_return_value_t = slice::spec_return_value_t;
tensor_return_value_t& output_tensor;

// AFTER: Direct types
using spec_return_value_t = TensorSpec;
Tensor& output_tensor;
```

## Usage

### Single Operation

```bash
# Process all files in an operation directory
clang-tidy-17 -load /path/to/TtNNReturnValueTypeAliasCheck.so \
  -checks='-*,ttnn-return-value-type-alias' \
  -fix-errors \
  -p /path/to/tt-metal/build \
  path/to/device/*.hpp path/to/device/*.cpp path/to/*.hpp path/to/*.cpp
```

### Batch Processing (Recommended)

Use the batch script in tt-metal:

```bash
# Dry run - see what would be changed
./run_return_value_alias_fix.sh --dry-run

# Dry run for specific operation
./run_return_value_alias_fix.sh --dry-run conv2d

# Apply fixes to all operations
./run_return_value_alias_fix.sh

# Apply fixes to specific operation
./run_return_value_alias_fix.sh conv2d
```

## Key Points

- **Process entire directories** - Run on all `.hpp` and `.cpp` files in the operation directory to catch all usages
- **Use `-fix-errors`** - This applies fixes even when there are transient compilation errors during the fix process
- **Compilation database required** - Use `-p` to point to the build directory with `compile_commands.json`

## Example

### Before

**types file:**
```cpp
namespace ttnn::operations::conv::conv2d {
using tensor_return_value_t = Tensor;
using spec_return_value_t = TensorSpec;
}
```

**device operation:**
```cpp
struct Conv2dDeviceOperation {
    using spec_return_value_t = conv2d::spec_return_value_t;
    using tensor_return_value_t = conv2d::tensor_return_value_t;
};
```

**program factory:**
```cpp
void create(..., tensor_return_value_t& output);
```

### After

**types file:**
```cpp
namespace ttnn::operations::conv::conv2d {
// Aliases removed
}
```

**device operation:**
```cpp
struct Conv2dDeviceOperation {
    using spec_return_value_t = TensorSpec;
    using tensor_return_value_t = Tensor;
};
```

**program factory:**
```cpp
void create(..., Tensor& output);
```
