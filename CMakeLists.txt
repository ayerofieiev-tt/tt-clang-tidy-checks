# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.15)
project(tt-clang-tidy-checks VERSION 1.0.0 LANGUAGES CXX)

# Allow user to specify clang version
set(CLANG_VERSION "17" CACHE STRING "Clang version to build against (e.g., 17, 18)")

# Option to download clang-tidy headers automatically
option(DOWNLOAD_CLANG_TIDY_HEADERS "Automatically download clang-tidy headers" ON)

# Clang-tidy plugins directory
# These plugins extend clang-tidy with TTNN-specific checks

add_subdirectory(ttnn-nanobind-overload)
add_subdirectory(ttnn-return-value-type-alias)
