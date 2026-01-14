# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

# Use CLANG_VERSION from parent or default to 17
if(NOT DEFINED CLANG_VERSION)
  set(CLANG_VERSION "17")
endif()

message(STATUS "Building ttnn-operation-type-naming plugin for Clang ${CLANG_VERSION}")

# Find required Clang components
set(CLANG_LIB_DIR "/usr/lib/llvm-${CLANG_VERSION}/lib")
set(CLANG_INCLUDE_DIR "/usr/lib/llvm-${CLANG_VERSION}/include")

# Check if shared libraries exist - try multiple locations and naming conventions
# Clang 17 uses libclang-cpp.so.17, Clang 20+ uses libclang-cpp.so.20.1 etc.
set(CLANG_CPP_LIB "")
foreach(TRY_LIB
    "${CLANG_LIB_DIR}/libclang-cpp.so.${CLANG_VERSION}"
    "${CLANG_LIB_DIR}/libclang-cpp.so.${CLANG_VERSION}.1"
    "/usr/lib/x86_64-linux-gnu/libclang-cpp.so.${CLANG_VERSION}"
    "/usr/lib/x86_64-linux-gnu/libclang-cpp.so.${CLANG_VERSION}.1")
  if(EXISTS "${TRY_LIB}")
    set(CLANG_CPP_LIB "${TRY_LIB}")
    break()
  endif()
endforeach()

# LLVM library - try multiple locations and names
set(LLVM_LIB "")
foreach(TRY_LIB
    "${CLANG_LIB_DIR}/libLLVM.so"
    "${CLANG_LIB_DIR}/libLLVM-${CLANG_VERSION}.so"
    "/usr/lib/x86_64-linux-gnu/libLLVM-${CLANG_VERSION}.so"
    "/usr/lib/x86_64-linux-gnu/libLLVM-${CLANG_VERSION}.so.1")
  if(EXISTS "${TRY_LIB}")
    set(LLVM_LIB "${TRY_LIB}")
    break()
  endif()
endforeach()

if(NOT CLANG_CPP_LIB OR NOT EXISTS "${CLANG_CPP_LIB}")
  message(FATAL_ERROR "Clang development libraries not found!\n"
    "  Install with: sudo apt-get install llvm-${CLANG_VERSION}-dev libclang-${CLANG_VERSION}-dev")
endif()

if(NOT LLVM_LIB OR NOT EXISTS "${LLVM_LIB}")
  message(FATAL_ERROR "LLVM library not found!\n"
    "  Install with: sudo apt-get install llvm-${CLANG_VERSION}-dev")
endif()

# Check if include directory exists
if(NOT EXISTS "${CLANG_INCLUDE_DIR}/clang/AST/ASTContext.h")
  # Try alternative locations
  foreach(TRY_DIR "/usr/include/clang/${CLANG_VERSION}" "/usr/include/clang/${CLANG_VERSION}.0.6")
    if(EXISTS "${TRY_DIR}")
      set(CLANG_INCLUDE_DIR "${TRY_DIR}/..")
      break()
    endif()
  endforeach()
endif()

if(NOT EXISTS "${CLANG_INCLUDE_DIR}/clang/AST/ASTContext.h")
  message(FATAL_ERROR "Clang include directory not found!\n"
    "  Install with: sudo apt-get install libclang-${CLANG_VERSION}-dev")
endif()

# Check for clang-tidy headers - these are NOT in Ubuntu packages
# We need to download them from LLVM source
set(CLANG_TIDY_HEADERS_DIR "${CMAKE_BINARY_DIR}/clang-tidy-headers")

if(DEFINED CLANG_TIDY_INCLUDE_DIR AND EXISTS "${CLANG_TIDY_INCLUDE_DIR}/clang-tidy/ClangTidy.h")
  # User provided the headers
  set(CLANG_TIDY_HEADERS_DIR "${CLANG_TIDY_INCLUDE_DIR}")
  message(STATUS "Using user-provided clang-tidy headers: ${CLANG_TIDY_HEADERS_DIR}")
elseif(DOWNLOAD_CLANG_TIDY_HEADERS)
  # Auto-download the headers
  # LLVM 17 uses 17.0.x, LLVM 18+ uses x.1.y versioning
  if(CLANG_VERSION EQUAL 17)
    set(LLVM_TAG "llvmorg-17.0.6")
  elseif(CLANG_VERSION EQUAL 18)
    set(LLVM_TAG "llvmorg-18.1.8")
  else()
    # For newer versions, try x.1.0 as default
    set(LLVM_TAG "llvmorg-${CLANG_VERSION}.1.0")
  endif()
  set(CLANG_TIDY_HEADER_URL "https://raw.githubusercontent.com/llvm/llvm-project/${LLVM_TAG}/clang-tools-extra/clang-tidy")

  # List of required headers
  set(CLANG_TIDY_HEADERS
    "ClangTidy.h"
    "ClangTidyCheck.h"
    "ClangTidyDiagnosticConsumer.h"
    "ClangTidyModule.h"
    "ClangTidyModuleRegistry.h"
    "ClangTidyOptions.h"
    "ClangTidyProfiling.h"
    "FileExtensionsSet.h"
    "GlobList.h"
    "NoLintDirectiveHandler.h"
  )

  file(MAKE_DIRECTORY "${CLANG_TIDY_HEADERS_DIR}/clang-tidy")

  set(HEADERS_DOWNLOADED TRUE)
  foreach(HEADER ${CLANG_TIDY_HEADERS})
    set(HEADER_PATH "${CLANG_TIDY_HEADERS_DIR}/clang-tidy/${HEADER}")
    if(NOT EXISTS "${HEADER_PATH}")
      message(STATUS "Downloading ${HEADER}...")
      file(DOWNLOAD
        "${CLANG_TIDY_HEADER_URL}/${HEADER}"
        "${HEADER_PATH}"
        STATUS DOWNLOAD_STATUS
        TIMEOUT 30
      )
      list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
      if(NOT STATUS_CODE EQUAL 0)
        message(WARNING "Failed to download ${HEADER}")
        set(HEADERS_DOWNLOADED FALSE)
      endif()
    endif()
  endforeach()

  if(NOT HEADERS_DOWNLOADED)
    message(FATAL_ERROR "Failed to download clang-tidy headers.\n"
      "  You can manually provide them with: -DCLANG_TIDY_INCLUDE_DIR=/path/to/llvm-project/clang-tools-extra")
  endif()

  message(STATUS "Downloaded clang-tidy headers to: ${CLANG_TIDY_HEADERS_DIR}")
else()
  message(FATAL_ERROR "Clang-tidy development headers not found!\n"
    "  Either enable DOWNLOAD_CLANG_TIDY_HEADERS=ON or provide:\n"
    "    -DCLANG_TIDY_INCLUDE_DIR=/path/to/llvm-project/clang-tools-extra")
endif()

message(STATUS "Found Clang shared libraries - building clang-tidy plugin")
message(STATUS "  CLANG_CPP_LIB: ${CLANG_CPP_LIB}")
message(STATUS "  LLVM_LIB: ${LLVM_LIB}")
message(STATUS "  CLANG_INCLUDE_DIR: ${CLANG_INCLUDE_DIR}")
message(STATUS "  CLANG_TIDY_HEADERS_DIR: ${CLANG_TIDY_HEADERS_DIR}")

# Collect source files
set(SOURCES
  TtNNOperationTypeNamingCheck.cpp
  Plugin.cpp
)

# Create the plugin library
add_library(TtNNOperationTypeNamingCheck MODULE ${SOURCES})

# Link against Clang shared libraries
target_link_libraries(TtNNOperationTypeNamingCheck
  PRIVATE
  ${CLANG_CPP_LIB}
  ${LLVM_LIB}
)

# Set C++ standard
target_compile_features(TtNNOperationTypeNamingCheck PRIVATE cxx_std_17)

# Include directories
target_include_directories(TtNNOperationTypeNamingCheck
  PRIVATE
  ${CLANG_INCLUDE_DIR}
  ${CLANG_TIDY_HEADERS_DIR}
)

# Set output name
set_target_properties(TtNNOperationTypeNamingCheck PROPERTIES
  PREFIX ""
  OUTPUT_NAME "TtNNOperationTypeNamingCheck"
)

# Install the plugin
install(TARGETS TtNNOperationTypeNamingCheck
  LIBRARY DESTINATION lib
)
