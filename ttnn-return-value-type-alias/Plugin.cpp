// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include "TtNNReturnValueTypeAliasCheck.h"
#include "clang-tidy/ClangTidyModule.h"
#include "clang-tidy/ClangTidyModuleRegistry.h"

using namespace clang::tidy;

namespace clang::tidy::ttnn {

class TtNNReturnValueTypeAliasModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<TtNNReturnValueTypeAliasCheck>(
        "ttnn-return-value-type-alias");
  }
};

} // namespace clang::tidy::ttnn

// Register the module
static ClangTidyModuleRegistry::Add<clang::tidy::ttnn::TtNNReturnValueTypeAliasModule> X(
    "ttnn-return-value-type-alias-module",
    "Adds check for return value type alias refactoring.");

// This anchor is used to force the linker to link in the generated object file
// and thus register the module.
volatile int TtNNReturnValueTypeAliasModuleAnchorSource = 0;
