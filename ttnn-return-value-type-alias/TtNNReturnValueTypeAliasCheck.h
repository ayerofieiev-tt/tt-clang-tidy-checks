// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#ifndef TTOOLS_CLANG_TIDY_PLUGINS_TTNN_RETURN_VALUE_TYPE_ALIAS_CHECK_H_
#define TTOOLS_CLANG_TIDY_PLUGINS_TTNN_RETURN_VALUE_TYPE_ALIAS_CHECK_H_

#include "clang-tidy/ClangTidy.h"
#include "clang-tidy/ClangTidyCheck.h"

namespace clang::tidy::ttnn {

/// Removes redundant return value type aliases and their usages.
///
/// In types files (*_device_operation_types.hpp):
///   - Removes `using spec_return_value_t = TensorSpec;`
///   - Removes `using tensor_return_value_t = Tensor;`
///
/// Everywhere else (device operation files, program factories, etc.):
///   - Replaces `namespace::spec_return_value_t` with `TensorSpec`
///   - Replaces `namespace::tensor_return_value_t` with `Tensor`
///
class TtNNReturnValueTypeAliasCheck : public ClangTidyCheck {
public:
  TtNNReturnValueTypeAliasCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace clang::tidy::ttnn

#endif // TTOOLS_CLANG_TIDY_PLUGINS_TTNN_RETURN_VALUE_TYPE_ALIAS_CHECK_H_
