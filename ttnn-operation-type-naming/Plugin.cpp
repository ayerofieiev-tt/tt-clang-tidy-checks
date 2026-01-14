// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include "TtNNOperationTypeNamingCheck.h"
#include "clang-tidy/ClangTidyModule.h"
#include "clang-tidy/ClangTidyModuleRegistry.h"

using namespace clang::tidy;

namespace clang::tidy::ttnn {

class TtNNOperationTypeNamingModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<TtNNOperationTypeNamingCheck>(
        "ttnn-operation-type-naming");
  }
};

} // namespace clang::tidy::ttnn

// Register the module
static ClangTidyModuleRegistry::Add<clang::tidy::ttnn::TtNNOperationTypeNamingModule> X(
    "ttnn-operation-type-naming-module",
    "Adds check for operation type naming conventions.");

// This anchor is used to force the linker to link in the generated object file
// and thus register the module.
volatile int TtNNOperationTypeNamingModuleAnchorSource = 0;
