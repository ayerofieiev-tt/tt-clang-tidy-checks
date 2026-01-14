// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include "TtNNReturnValueTypeAliasCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::ttnn {

namespace {

// Target type alias names
constexpr const char *kSpecReturnValueT = "spec_return_value_t";
constexpr const char *kTensorReturnValueT = "tensor_return_value_t";

// Check if the file is a types file (*_device_operation_types.hpp)
bool isTypesFile(llvm::StringRef Filename) {
  return Filename.ends_with("_device_operation_types.hpp");
}

// Check if a type string refers to a namespace-level alias (not from a struct)
// Returns the alias name if found, empty string otherwise
std::string getTargetAliasFromType(const std::string &TypeStr) {
  // Skip if this is from a struct/class (contains "DeviceOperation::" or similar)
  // We only want namespace-level aliases
  if (TypeStr.find("DeviceOperation::") != std::string::npos ||
      TypeStr.find("Operation::") != std::string::npos) {
    return "";
  }

  if (TypeStr.find("::" + std::string(kSpecReturnValueT)) != std::string::npos) {
    return kSpecReturnValueT;
  }
  if (TypeStr.find("::" + std::string(kTensorReturnValueT)) != std::string::npos) {
    return kTensorReturnValueT;
  }
  return "";
}

// Get the replacement type for an alias
std::string getReplacementType(const std::string &AliasName) {
  if (AliasName == kSpecReturnValueT) {
    return "TensorSpec";
  } else if (AliasName == kTensorReturnValueT) {
    return "Tensor";
  }
  return "";
}

// Check if the underlying type is a direct type (TensorSpec, Tensor)
bool isDirectTypeDefinition(const clang::QualType &QT) {
  std::string TypeStr = QT.getAsString();
  // Strip any namespace prefixes for comparison
  return TypeStr == "TensorSpec" || TypeStr == "Tensor" ||
         TypeStr == "ttnn::TensorSpec" || TypeStr == "ttnn::Tensor" ||
         TypeStr == "class TensorSpec" || TypeStr == "class Tensor" ||
         TypeStr == "class ttnn::TensorSpec" || TypeStr == "class ttnn::Tensor";
}

// Get the source range for the line containing the declaration (for removal)
clang::CharSourceRange getLineRange(const clang::TypeAliasDecl *TAD,
                                     const clang::SourceManager &SM,
                                     const clang::LangOptions &LO) {
  clang::SourceLocation Start = TAD->getBeginLoc();
  clang::SourceLocation End = TAD->getEndLoc();

  // Find the semicolon
  clang::SourceLocation SemiLoc = clang::Lexer::findLocationAfterToken(
      End, clang::tok::semi, SM, LO, false);

  if (SemiLoc.isInvalid()) {
    SemiLoc = End;
  }

  // Try to include the newline after the semicolon
  clang::FileID FID = SM.getFileID(Start);
  unsigned SemiOffset = SM.getFileOffset(SemiLoc);

  bool Invalid = false;
  llvm::StringRef Buffer = SM.getBufferData(FID, &Invalid);
  if (!Invalid && SemiOffset < Buffer.size()) {
    // Skip any whitespace after semicolon, include the newline
    unsigned EndOffset = SemiOffset;
    while (EndOffset < Buffer.size() && Buffer[EndOffset] != '\n') {
      EndOffset++;
    }
    if (EndOffset < Buffer.size() && Buffer[EndOffset] == '\n') {
      EndOffset++; // Include the newline
    }

    // Also try to remove leading whitespace on the line
    unsigned StartOffset = SM.getFileOffset(Start);
    unsigned LineStart = StartOffset;
    while (LineStart > 0 && Buffer[LineStart - 1] != '\n') {
      LineStart--;
    }
    // Check if the content before Start on this line is only whitespace
    bool OnlyWhitespace = true;
    for (unsigned i = LineStart; i < StartOffset; ++i) {
      if (!std::isspace(Buffer[i])) {
        OnlyWhitespace = false;
        break;
      }
    }
    if (OnlyWhitespace) {
      Start = SM.getLocForStartOfFile(FID).getLocWithOffset(LineStart);
    }

    clang::SourceLocation EndLoc = SM.getLocForStartOfFile(FID).getLocWithOffset(EndOffset);
    return clang::CharSourceRange::getCharRange(Start, EndLoc);
  }

  return clang::CharSourceRange::getTokenRange(Start, SemiLoc);
}

} // namespace

void TtNNReturnValueTypeAliasCheck::registerMatchers(MatchFinder *Finder) {
  // Case 1: Match type alias declarations in types files (using X = Tensor;)
  Finder->addMatcher(
      typeAliasDecl().bind("type_alias_decl"),
      this);

  // Case 2: Match any usage of namespace::spec_return_value_t or namespace::tensor_return_value_t
  // This catches usages in function parameters, return types, variable declarations, etc.
  Finder->addMatcher(
      typeLoc().bind("type_loc"),
      this);
}

void TtNNReturnValueTypeAliasCheck::check(
    const MatchFinder::MatchResult &Result) {
  const clang::SourceManager &SM = *Result.SourceManager;
  const clang::LangOptions &LO = getLangOpts();

  // Handle type alias declarations (for removal from types files)
  if (const auto *TAD = Result.Nodes.getNodeAs<clang::TypeAliasDecl>("type_alias_decl")) {
    if (!SM.isInMainFile(TAD->getLocation())) {
      return;
    }

    llvm::StringRef AliasName = TAD->getName();
    if (AliasName != kSpecReturnValueT && AliasName != kTensorReturnValueT) {
      return;
    }

    llvm::StringRef Filename = SM.getFilename(TAD->getLocation());
    clang::QualType UnderlyingType = TAD->getUnderlyingType();
    const clang::DeclContext *DC = TAD->getDeclContext();
    bool IsInStruct = DC && (isa<clang::CXXRecordDecl>(DC) || isa<clang::RecordDecl>(DC));

    // Only flag namespace-level aliases in types files that directly alias Tensor/TensorSpec
    if (isTypesFile(Filename) && !IsInStruct && isDirectTypeDefinition(UnderlyingType)) {
      auto Diag = diag(TAD->getLocation(),
                       "redundant type alias '%0'; remove from types file")
          << AliasName;

      clang::CharSourceRange RemoveRange = getLineRange(TAD, SM, LO);
      Diag << clang::FixItHint::CreateRemoval(RemoveRange);
    }
    return;
  }

  // Handle type usages (replace namespace::tensor_return_value_t with Tensor)
  if (const auto *TL = Result.Nodes.getNodeAs<clang::TypeLoc>("type_loc")) {
    if (!SM.isInMainFile(TL->getBeginLoc())) {
      return;
    }

    // Get the type as string
    clang::QualType QT = TL->getType();
    if (QT.isNull()) {
      return;
    }

    std::string TypeStr = QT.getAsString();
    std::string AliasName = getTargetAliasFromType(TypeStr);
    if (AliasName.empty()) {
      return;
    }

    // Skip if this is from a type alias declaration in types file (handled above)
    // We only want to fix usages, not the definition
    llvm::StringRef Filename = SM.getFilename(TL->getBeginLoc());
    if (isTypesFile(Filename)) {
      return;
    }

    std::string ReplacementType = getReplacementType(AliasName);
    if (ReplacementType.empty()) {
      return;
    }

    // Get the source range for the type
    clang::SourceRange Range = TL->getSourceRange();
    if (Range.isInvalid()) {
      return;
    }

    auto Diag = diag(TL->getBeginLoc(),
                     "replace '%0' with '%1'")
        << TypeStr << ReplacementType;

    Diag << clang::FixItHint::CreateReplacement(Range, ReplacementType);
  }
}

} // namespace clang::tidy::ttnn
