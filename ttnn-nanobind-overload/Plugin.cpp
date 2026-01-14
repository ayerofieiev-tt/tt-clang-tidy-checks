// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include "TtNNNanobindOverloadCheck.h"
#include "clang-tidy/ClangTidyModule.h"
#include "clang-tidy/ClangTidyModuleRegistry.h"

using namespace clang::tidy;

namespace clang::tidy::ttnn {

class TtNNModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<TtNNNanobindOverloadCheck>(
        "ttnn-nanobind-unnecessary-overload");
  }
};

} // namespace clang::tidy::ttnn

// Register the module
static ClangTidyModuleRegistry::Add<clang::tidy::ttnn::TtNNModule> X(
    "ttnn-module", "Adds TTNN-specific clang-tidy checks.");

// This anchor is used to force the linker to link in the generated object file
// and thus register the module.
volatile int TtNNModuleAnchorSource = 0;
