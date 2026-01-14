// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#ifndef TTOOLS_CLANG_TIDY_PLUGINS_TTNN_NANOBIND_OVERLOAD_TTNNNANOBINDOVERLOADCHECK_H_
#define TTOOLS_CLANG_TIDY_PLUGINS_TTNN_NANOBIND_OVERLOAD_TTNNNANOBINDOVERLOADCHECK_H_

#include "clang-tidy/ClangTidy.h"
#include "clang-tidy/ClangTidyCheck.h"

namespace clang::tidy::ttnn {

/// Finds cases where `nanobind_overload_t` is used unnecessarily in
/// `bind_registered_operation` calls.
///
/// When there's only one overload, `nanobind_arguments_t` should be used
/// instead of `nanobind_overload_t`.
///
/// For the user-facing documentation see:
/// https://clang.llvm.org/extra/clang-tidy/checks/ttnn/nanobind-unnecessary-overload.html
class TtNNNanobindOverloadCheck : public ClangTidyCheck {
public:
  TtNNNanobindOverloadCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace clang::tidy::ttnn

#endif // TTOOLS_CLANG_TIDY_PLUGINS_TTNN_NANOBIND_OVERLOAD_TTNNNANOBINDOVERLOADCHECK_H_
