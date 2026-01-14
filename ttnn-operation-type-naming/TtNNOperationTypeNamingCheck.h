// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#ifndef TTOOLS_CLANG_TIDY_PLUGINS_TTNN_OPERATION_TYPE_NAMING_CHECK_H_
#define TTOOLS_CLANG_TIDY_PLUGINS_TTNN_OPERATION_TYPE_NAMING_CHECK_H_

#include "clang-tidy/ClangTidy.h"
#include "clang-tidy/ClangTidyCheck.h"

namespace clang::tidy::ttnn {

/// Detects generic operation type names and suggests operation-specific names.
///
/// In types files (*_device_operation_types.hpp):
///   - Flags `struct operation_attributes_t { ... };` -> suggests `{Operation}Params`
///   - Flags `struct tensor_args_t { ... };` -> suggests `{Operation}Inputs`
///
/// The operation name is derived from the namespace (e.g., `slice` -> `Slice`).
///
class TtNNOperationTypeNamingCheck : public ClangTidyCheck {
public:
  TtNNOperationTypeNamingCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace clang::tidy::ttnn

#endif // TTOOLS_CLANG_TIDY_PLUGINS_TTNN_OPERATION_TYPE_NAMING_CHECK_H_
