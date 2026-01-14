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

// Helper function to check if a type name contains "nanobind_overload_t"
bool isNanobindOverloadTType(const clang::Type *Type) {
  if (!Type) {
    return false;
  }

  // Check template specialization
  if (const auto *TemplateType = Type->getAs<clang::TemplateSpecializationType>()) {
    const auto *TemplateDecl = TemplateType->getTemplateName().getAsTemplateDecl();
    if (TemplateDecl) {
      // Check unqualified name
      std::string TypeName = TemplateDecl->getNameAsString();
      if (TypeName.find("nanobind_overload_t") != std::string::npos) {
        return true;
      }

      // Check qualified name
      if (const auto *NamedDecl = dyn_cast<clang::NamedDecl>(TemplateDecl)) {
        std::string QualifiedName = NamedDecl->getQualifiedNameAsString();
        if (QualifiedName.find("nanobind_overload_t") != std::string::npos ||
            QualifiedName.find("ttnn::nanobind_overload_t") != std::string::npos) {
          return true;
        }
      }
    }
  }

  // Check typedef/type alias
  if (const auto *TypedefType = Type->getAs<clang::TypedefType>()) {
    const auto *TypedefDecl = TypedefType->getDecl();
    std::string TypedefName = TypedefDecl->getNameAsString();
    if (TypedefName.find("nanobind_overload_t") != std::string::npos) {
      return true;
    }
    // Check underlying type
    return isNanobindOverloadTType(TypedefDecl->getUnderlyingType().getTypePtr());
  }

  // Check for ElaboratedType (e.g., "ttnn::nanobind_overload_t")
  if (const auto *ElaboratedType = Type->getAs<clang::ElaboratedType>()) {
    return isNanobindOverloadTType(ElaboratedType->getNamedType().getTypePtr());
  }

  return false;
}

// Helper function to check if an expression is a nanobind_overload_t
bool isNanobindOverloadTExpr(const clang::Expr *E) {
  if (!E) {
    return false;
  }

  // Simple approach: check if the type name contains "nanobind_overload_t"
  std::string TypeName = E->getType().getAsString();
  return TypeName.find("nanobind_overload_t") != std::string::npos;
}

// Helper to get the source text for a range
std::string getSourceText(clang::SourceRange Range, const clang::SourceManager &SM,
                          const clang::LangOptions &LO) {
  if (Range.isInvalid()) {
    return "";
  }
  bool Invalid = false;
  std::string Text = clang::Lexer::getSourceText(
      clang::CharSourceRange::getTokenRange(Range), SM, LO, &Invalid).str();
  if (Invalid) {
    return "";
  }
  return Text;
}

// Helper to find the end of a lambda expression
clang::SourceLocation findLambdaEnd(const clang::Expr *LambdaExpr,
                                     const clang::SourceManager &SM,
                                     const clang::LangOptions &LO) {
  if (!LambdaExpr) {
    return clang::SourceLocation();
  }

  // For lambda expressions, find the closing brace
  if (const auto *Lambda = dyn_cast<clang::LambdaExpr>(LambdaExpr)) {
    return Lambda->getEndLoc();
  }

  // For other expressions, use their end location
  return LambdaExpr->getEndLoc();
}

// Generate fix for a single nanobind_overload_t argument
void generateFixForOverload(const clang::Expr *OverloadExpr,
                            const clang::SourceManager &SM,
                            const clang::LangOptions &LO,
                            clang::DiagnosticBuilder &Diag) {
  if (!OverloadExpr) {
    return;
  }

  // Find the constructor expression
  const clang::CXXConstructExpr *ConstructExpr = nullptr;
  if (const auto *CE = dyn_cast<clang::CXXConstructExpr>(OverloadExpr)) {
    ConstructExpr = CE;
  } else if (const auto *TempExpr = dyn_cast<clang::CXXTemporaryObjectExpr>(OverloadExpr)) {
    ConstructExpr = TempExpr;
  }

  if (!ConstructExpr || ConstructExpr->getNumArgs() < 2) {
    // Need at least lambda + one argument
    return;
  }

  // Get the lambda (first argument)
  const clang::Expr *LambdaArg = ConstructExpr->getArg(0)->IgnoreImplicit();
  if (!LambdaArg) {
    return;
  }

  clang::SourceLocation LambdaStart = LambdaArg->getBeginLoc();
  clang::SourceLocation LambdaEnd = findLambdaEnd(LambdaArg, SM, LO);

  if (LambdaStart.isInvalid() || LambdaEnd.isInvalid()) {
    return;
  }

  // Find the comma after the lambda
  clang::SourceLocation CommaLoc = clang::Lexer::findLocationAfterToken(
      LambdaEnd, clang::tok::comma, SM, LO, false);

  // Determine the end of what to remove (lambda + comma + whitespace)
  clang::SourceLocation RemoveEnd = LambdaEnd;
  if (CommaLoc.isValid()) {
    RemoveEnd = CommaLoc;
    // Skip whitespace after comma to preserve formatting
    clang::SourceLocation AfterComma = clang::Lexer::getLocForEndOfToken(CommaLoc, 0, SM, LO);
    if (AfterComma.isValid()) {
      // Check if there's a newline - if so, include it in removal
      const char *AfterCommaData = SM.getCharacterData(AfterComma);
      if (AfterCommaData && (*AfterCommaData == '\n' || *AfterCommaData == '\r')) {
        RemoveEnd = AfterComma.getLocWithOffset(1);
      } else {
        // Include one space if present
        if (*AfterCommaData == ' ') {
          RemoveEnd = AfterComma.getLocWithOffset(1);
        }
      }
    }
  } else {
    // No comma found, just remove to end of lambda
    RemoveEnd = clang::Lexer::getLocForEndOfToken(LambdaEnd, 0, SM, LO);
  }

  // Find the type name location by searching in the source text
  clang::SourceLocation ExprStart = OverloadExpr->getBeginLoc();
  if (ExprStart.isInvalid()) {
    return;
  }

  // Read source text starting from the expression
  const char *StartData = SM.getCharacterData(ExprStart);
  if (!StartData) {
    return;
  }

  // Search for "nanobind_overload_t" in the source
  std::string SearchText(StartData, std::min(200UL, strlen(StartData)));
  size_t OverloadPos = SearchText.find("nanobind_overload_t");
  if (OverloadPos == std::string::npos) {
    return;
  }

  // Check if it's qualified with "ttnn::"
  bool HasQualifier = (OverloadPos >= 5 && 
                       SearchText.substr(OverloadPos - 5, 5) == "ttnn::");
  
  // Calculate the actual character positions
  const char *TypeNameStartChar = StartData + (OverloadPos - (HasQualifier ? 5 : 0));
  const char *TypeNameEndChar = StartData + OverloadPos + strlen("nanobind_overload_t");

  // Convert character pointers to source locations
  // We need to find the file location
  clang::FileID FID = SM.getFileID(ExprStart);
  unsigned Offset = SM.getFileOffset(ExprStart);
  unsigned TypeNameStartOffset = Offset + (TypeNameStartChar - StartData);
  unsigned TypeNameEndOffset = Offset + (TypeNameEndChar - StartData);

  clang::SourceLocation TypeNameStart = SM.getLocForStartOfFile(FID);
  TypeNameStart = TypeNameStart.getLocWithOffset(TypeNameStartOffset);
  clang::SourceLocation TypeNameEnd = SM.getLocForStartOfFile(FID);
  TypeNameEnd = TypeNameEnd.getLocWithOffset(TypeNameEndOffset);

  if (TypeNameStart.isInvalid() || TypeNameEnd.isInvalid()) {
    return;
  }

  // Generate fixes:
  // 1. Replace "nanobind_overload_t" (or "ttnn::nanobind_overload_t") with "ttnn::nanobind_arguments_t"
  std::string Replacement = HasQualifier ? "ttnn::nanobind_arguments_t" : "nanobind_arguments_t";
  Diag << clang::FixItHint::CreateReplacement(
      clang::SourceRange(TypeNameStart, TypeNameEnd), Replacement);

  // 2. Remove the lambda and its trailing comma/whitespace
  if (RemoveEnd.isValid() && RemoveEnd != LambdaStart) {
    Diag << clang::FixItHint::CreateRemoval(
        clang::SourceRange(LambdaStart, RemoveEnd));
  }
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
