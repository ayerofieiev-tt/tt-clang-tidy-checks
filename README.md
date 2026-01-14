# TT Clang-Tidy Checks

Custom clang-tidy checks for Tenstorrent codebases.

## Available Checks

### `ttnn-nanobind-unnecessary-overload`

Detects unnecessary use of `nanobind_overload_t` when only a single overload is provided in `bind_registered_operation` calls. Suggests using `nanobind_arguments_t` instead.

See [ttnn-nanobind-overload/README.md](ttnn-nanobind-overload/README.md) for details.

## Quick Start

### Using Pre-built Releases

Download the pre-built plugin for your clang version from the [Releases](https://github.com/ayerofieiev-tt/tt-clang-tidy-checks/releases) page.

```bash
# Download the plugin (example for clang-17)
wget https://github.com/ayerofieiev-tt/tt-clang-tidy-checks/releases/download/v1.0.0/TtNNNanobindOverloadCheck-clang17.so

# Run on a file
clang-tidy-17 -load ./TtNNNanobindOverloadCheck-clang17.so \
  -checks='-*,ttnn-nanobind-unnecessary-overload' \
  your_file.cpp -- -std=c++20 [other flags]
```

### Building from Source

#### Prerequisites

- CMake 3.15+
- Clang/LLVM development libraries

```bash
# Ubuntu/Debian - Install Clang 17 dev libraries
sudo apt-get install llvm-17-dev libclang-17-dev clang-17

# Or for Clang 18
sudo apt-get install llvm-18-dev libclang-18-dev clang-18
```

#### Build

```bash
git clone https://github.com/ayerofieiev-tt/tt-clang-tidy-checks.git
cd tt-clang-tidy-checks

mkdir build && cd build

# For Clang 17 (default)
cmake ..

# Or specify a different Clang version
cmake .. -DCLANG_VERSION=18

# Build
make -j$(nproc)
```

The plugin will be built as `ttnn-nanobind-overload/TtNNNanobindOverloadCheck.so`.

#### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CLANG_VERSION` | `17` | Clang version to build against |
| `DOWNLOAD_CLANG_TIDY_HEADERS` | `ON` | Auto-download clang-tidy headers from LLVM repo |
| `CLANG_TIDY_INCLUDE_DIR` | - | Path to clang-tidy headers (if not auto-downloading) |

## Usage

### Basic Usage

```bash
clang-tidy-17 -load /path/to/TtNNNanobindOverloadCheck.so \
  -checks='-*,ttnn-nanobind-unnecessary-overload' \
  source_file.cpp \
  -- -std=c++20 -I/include/paths
```

### With compile_commands.json

```bash
clang-tidy-17 -load /path/to/TtNNNanobindOverloadCheck.so \
  -checks='-*,ttnn-nanobind-unnecessary-overload' \
  -p /path/to/build \
  source_file.cpp
```

### Auto-fix

```bash
clang-tidy-17 -load /path/to/TtNNNanobindOverloadCheck.so \
  -checks='-*,ttnn-nanobind-unnecessary-overload' \
  --fix \
  -p /path/to/build \
  source_file.cpp
```

## Example Output

```
source.cpp:53:5: warning: unnecessary use of nanobind_overload_t with a single overload; use nanobind_arguments_t instead [ttnn-nanobind-unnecessary-overload]
   53 |     bind_registered_operation(
      |     ^
```

## License

Apache-2.0
