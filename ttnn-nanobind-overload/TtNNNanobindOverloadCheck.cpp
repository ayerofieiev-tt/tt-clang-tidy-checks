// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include "TtNNNanobindOverloadCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::ttnn {

namespace {

// Helper function to check if an expression is a nanobind_overload_t
bool isNanobindOverloadTExpr(const clang::Expr *E) {
  if (!E) {
    return false;
  }

  // Simple approach: check if the type name contains "nanobind_overload_t"
  std::string TypeName = E->getType().getAsString();
  return TypeName.find("nanobind_overload_t") != std::string::npos;
}

// Generate fix for a single nanobind_overload_t argument
void generateFixForOverload(const clang::Expr *OverloadExpr,
                            const clang::SourceManager &SM,
                            const clang::LangOptions &LO,
                            clang::DiagnosticBuilder &Diag) {
  if (!OverloadExpr) {
    return;
  }

  // We need CXXTemporaryObjectExpr to get proper type location
  const auto *TempExpr = dyn_cast<clang::CXXTemporaryObjectExpr>(OverloadExpr);
  if (!TempExpr || TempExpr->getNumArgs() < 2) {
    return;
  }

  // Get the type source info - this gives us the exact location of the type name
  clang::TypeSourceInfo *TSI = TempExpr->getTypeSourceInfo();
  if (!TSI) {
    return;
  }

  // Get the type location - find "nanobind_overload_t" and replace with "nanobind_arguments_t"
  clang::SourceLocation TypeStart = TSI->getTypeLoc().getBeginLoc();
  if (TypeStart.isInvalid()) {
    return;
  }

  // Read the source text to find "nanobind_overload_t"
  const char *TypeData = SM.getCharacterData(TypeStart);
  if (!TypeData) {
    return;
  }

  // Search for "nanobind_overload_t" in the source
  std::string TypeText(TypeData, std::min(size_t(100), strlen(TypeData)));
  size_t NanobindPos = TypeText.find("nanobind_overload_t");
  if (NanobindPos == std::string::npos) {
    return;
  }

  // "nanobind_overload_t" is 19 characters
  // Use CharSourceRange for precise character-level replacement
  clang::SourceLocation ReplaceStart = TypeStart.getLocWithOffset(NanobindPos);
  clang::SourceLocation ReplaceEnd = TypeStart.getLocWithOffset(NanobindPos + 19); // one past the end

  // Fix 1: Replace "nanobind_overload_t" with "nanobind_arguments_t"
  Diag << clang::FixItHint::CreateReplacement(
      clang::CharSourceRange::getCharRange(ReplaceStart, ReplaceEnd),
      "nanobind_arguments_t");

  // Fix 2: Remove any "using OperationType = decltype(...);" line before this expression
  // Search backward from the type start for this pattern
  clang::SourceLocation SearchLoc = TypeStart;
  unsigned SearchOffset = SM.getFileOffset(SearchLoc);
  clang::FileID FID = SM.getFileID(SearchLoc);

  // Get buffer data and search backward for "using OperationType"
  bool InvalidBuf = false;
  llvm::StringRef Buffer = SM.getBufferData(FID, &InvalidBuf);
  if (!InvalidBuf && SearchOffset > 0) {
    // Search backward up to 500 chars for "using OperationType"
    size_t SearchStart = (SearchOffset > 500) ? SearchOffset - 500 : 0;
    std::string SearchRegion = Buffer.substr(SearchStart, SearchOffset - SearchStart).str();

    size_t UsingPos = SearchRegion.rfind("using OperationType");
    if (UsingPos != std::string::npos) {
      // Find the start of this line (go back to previous newline)
      size_t LineStart = SearchRegion.rfind('\n', UsingPos);
      if (LineStart == std::string::npos) {
        LineStart = 0;
      } else {
        LineStart++; // Skip the newline itself
      }

      // Find the end of this line (find next newline after UsingPos)
      size_t LineEnd = SearchRegion.find('\n', UsingPos);
      if (LineEnd == std::string::npos) {
        LineEnd = SearchRegion.size();
      } else {
        LineEnd++; // Include the newline in removal
      }

      // Create source locations for the line to remove
      clang::SourceLocation RemoveLineStart = SM.getLocForStartOfFile(FID).getLocWithOffset(SearchStart + LineStart);
      clang::SourceLocation RemoveLineEnd = SM.getLocForStartOfFile(FID).getLocWithOffset(SearchStart + LineEnd);

      Diag << clang::FixItHint::CreateRemoval(
          clang::CharSourceRange::getCharRange(RemoveLineStart, RemoveLineEnd));
    }
  }

  // Fix 3: Remove the lambda (first argument) and its trailing comma
  const clang::Expr *LambdaArg = TempExpr->getArg(0)->IgnoreImplicit();
  const clang::Expr *NextArg = TempExpr->getArg(1)->IgnoreImplicit();

  if (!LambdaArg || !NextArg) {
    return;
  }

  clang::SourceLocation LambdaStart = LambdaArg->getBeginLoc();
  clang::SourceLocation LambdaEnd = LambdaArg->getEndLoc();

  if (LambdaStart.isInvalid() || LambdaEnd.isInvalid()) {
    return;
  }

  // Find the comma after the lambda to verify structure
  clang::SourceLocation CommaLoc = clang::Lexer::findLocationAfterToken(
      LambdaEnd, clang::tok::comma, SM, LO, false);

  if (CommaLoc.isInvalid()) {
    return;
  }

  // Remove from lambda start through the comma
  // Then skip any whitespace/newlines to align with the next argument
  clang::SourceLocation NextArgStart = NextArg->getBeginLoc();

  // We want to remove: "lambda_expr," and any whitespace up to next arg
  // But preserve the newline + indentation of the next arg
  // So remove from LambdaStart to just before NextArgStart
  clang::SourceLocation RemoveEnd = NextArgStart.getLocWithOffset(-1);

  Diag << clang::FixItHint::CreateRemoval(
      clang::SourceRange(LambdaStart, RemoveEnd));
}

} // namespace

void TtNNNanobindOverloadCheck::registerMatchers(MatchFinder *Finder) {
  // Match all call expressions - we filter in check()
  Finder->addMatcher(
      callExpr().bind("bind_call"),
      this);
}

void TtNNNanobindOverloadCheck::check(
    const MatchFinder::MatchResult &Result) {
  const auto *Call = Result.Nodes.getNodeAs<clang::CallExpr>("bind_call");
  if (!Call) {
    return;
  }

  const clang::SourceManager &SM = *Result.SourceManager;
  const clang::LangOptions &LO = getLangOpts();

  // Only check calls in the main file
  if (!SM.isInMainFile(Call->getBeginLoc())) {
    return;
  }

  // Check if this is a call to bind_registered_operation
  const clang::FunctionDecl *FD = Call->getDirectCallee();
  if (!FD) {
    return;
  }
  std::string FuncName = FD->getQualifiedNameAsString();
  if (FuncName.find("bind_registered_operation") == std::string::npos) {
    return;
  }

  // Count nanobind_overload_t arguments and find the one to fix
  // Arguments start at index 3 (after mod, operation, doc)
  unsigned int OverloadCount = 0;
  unsigned int ArgIndex = 3; // Start checking from 4th argument (0-indexed)
  const clang::Expr *OverloadToFix = nullptr;

  for (unsigned int i = ArgIndex; i < Call->getNumArgs(); ++i) {
    const clang::Expr *Arg = Call->getArg(i)->IgnoreImplicit();
    if (isNanobindOverloadTExpr(Arg)) {
      OverloadCount++;
      if (!OverloadToFix) {
        OverloadToFix = Arg;
      }
    }
  }

  // If exactly one overload is found, it's unnecessary
  if (OverloadCount == 1 && OverloadToFix) {
    auto Diag = diag(Call->getBeginLoc(),
                     "unnecessary use of nanobind_overload_t with a single overload; "
                     "use nanobind_arguments_t instead");
    
    // Generate the auto-fix
    generateFixForOverload(OverloadToFix, SM, LO, Diag);
  }
}

} // namespace clang::tidy::ttnn
