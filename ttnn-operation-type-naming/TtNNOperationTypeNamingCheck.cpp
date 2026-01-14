// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include "TtNNOperationTypeNamingCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

#include <cctype>

using namespace clang::ast_matchers;

namespace clang::tidy::ttnn {

namespace {

// Target struct names that should be renamed
constexpr const char *kOperationAttributesT = "operation_attributes_t";
constexpr const char *kTensorArgsT = "tensor_args_t";

// Check if the file is a types file (*_device_operation_types.hpp)
bool isTypesFile(llvm::StringRef Filename) {
  return Filename.ends_with("_device_operation_types.hpp");
}

// Convert snake_case or lowercase to PascalCase
// e.g., "slice" -> "Slice", "conv2d" -> "Conv2d", "batch_norm" -> "BatchNorm"
std::string toPascalCase(llvm::StringRef Name) {
  std::string Result;
  bool CapitalizeNext = true;

  for (char C : Name) {
    if (C == '_') {
      CapitalizeNext = true;
    } else {
      if (CapitalizeNext) {
        Result += std::toupper(static_cast<unsigned char>(C));
        CapitalizeNext = false;
      } else {
        Result += C;
      }
    }
  }

  return Result;
}

// Extract the operation name from the namespace context
// Returns the innermost namespace name that's not "operations", "data_movement", etc.
std::string extractOperationName(const clang::DeclContext *DC) {
  // Collect namespace names from innermost to outermost
  std::vector<std::string> NamespaceNames;

  while (DC) {
    if (const auto *NS = dyn_cast<clang::NamespaceDecl>(DC)) {
      std::string NSName = NS->getNameAsString();
      if (!NSName.empty()) {
        NamespaceNames.push_back(NSName);
      }
    }
    DC = DC->getParent();
  }

  // The innermost namespace should be the operation name
  // Typical pattern: ttnn::operations::<category>::<operation>
  // e.g., ttnn::operations::data_movement::slice
  // We want "slice"
  if (!NamespaceNames.empty()) {
    // Skip common namespace names to find the actual operation
    static const std::set<std::string> CommonNames = {
        "ttnn", "operations", "data_movement", "eltwise", "binary", "unary",
        "reduction", "matmul", "conv", "pool", "normalization", "transformer",
        "embedding", "loss", "kv_cache", "ccl", "moreh", "experimental",
        "creation", "copy", "reshape_common", "reshape_on_device", "program"};

    // Return the first (innermost) namespace that's not a common category
    for (const auto &Name : NamespaceNames) {
      if (CommonNames.find(Name) == CommonNames.end()) {
        return Name;
      }
    }

    // Fallback: just use the innermost namespace
    return NamespaceNames[0];
  }

  return "";
}

// Get the suggested replacement name for the struct
// operation_attributes_t -> {Operation}Params
// tensor_args_t -> {Operation}Inputs
std::string getSuggestedName(llvm::StringRef CurrentName,
                             const std::string &OperationName) {
  std::string PascalOp = toPascalCase(OperationName);

  if (CurrentName == kOperationAttributesT) {
    return PascalOp + "Params";
  } else if (CurrentName == kTensorArgsT) {
    return PascalOp + "Inputs";
  }

  return "";
}

// Extract operation name from a fully qualified type string
// e.g., "ttnn::operations::data_movement::slice::operation_attributes_t" -> "slice"
// e.g., "struct ttnn::operations::data_movement::slice::operation_attributes_t" -> "slice"
std::string extractOperationFromFullyQualifiedType(const std::string &TypeStr) {
  // Look for operation_attributes_t or tensor_args_t in the type string
  std::string TargetType;
  size_t TypePos = TypeStr.find(kOperationAttributesT);
  if (TypePos != std::string::npos) {
    TargetType = kOperationAttributesT;
  } else {
    TypePos = TypeStr.find(kTensorArgsT);
    if (TypePos != std::string::npos) {
      TargetType = kTensorArgsT;
    }
  }

  if (TargetType.empty() || TypePos == 0) {
    return "";
  }

  // Look for "::" before the type name to find the namespace
  if (TypePos < 2 || TypeStr.substr(TypePos - 2, 2) != "::") {
    return "";
  }

  // Find the namespace before ::operation_attributes_t or ::tensor_args_t
  size_t NamespaceEnd = TypePos - 2;
  size_t NamespaceStart = NamespaceEnd;

  // Walk backwards to find the start of the namespace
  while (NamespaceStart > 0) {
    char C = TypeStr[NamespaceStart - 1];
    if (C == ':' || C == ' ') {
      break;
    }
    NamespaceStart--;
  }

  if (NamespaceStart >= NamespaceEnd) {
    return "";
  }

  return TypeStr.substr(NamespaceStart, NamespaceEnd - NamespaceStart);
}

// Check if a type represents one of our target types
bool isTargetType(const std::string &TypeStr) {
  // Skip DeviceOperation:: member types - these are the aliases themselves
  if (TypeStr.find("DeviceOperation::") != std::string::npos) {
    return false;
  }

  // Look for fully qualified names containing our target types
  return TypeStr.find("::" + std::string(kOperationAttributesT)) != std::string::npos ||
         TypeStr.find("::" + std::string(kTensorArgsT)) != std::string::npos;
}

// Get the target type name from a type string
std::string getTargetTypeName(const std::string &TypeStr) {
  if (TypeStr.find(kOperationAttributesT) != std::string::npos) {
    return kOperationAttributesT;
  }
  if (TypeStr.find(kTensorArgsT) != std::string::npos) {
    return kTensorArgsT;
  }
  return "";
}

} // namespace

void TtNNOperationTypeNamingCheck::registerMatchers(MatchFinder *Finder) {
  // Case 1: Match struct/class definitions with the target names (for renaming in types files)
  Finder->addMatcher(
      cxxRecordDecl(isDefinition(),
                    anyOf(hasName(kOperationAttributesT), hasName(kTensorArgsT)))
          .bind("struct_decl"),
      this);

  // Case 2: Match type usages (for updating references to the renamed types)
  Finder->addMatcher(typeLoc().bind("type_loc"), this);
}

void TtNNOperationTypeNamingCheck::check(
    const MatchFinder::MatchResult &Result) {
  const clang::SourceManager &SM = *Result.SourceManager;

  // Handle struct definitions (rename in types files)
  if (const auto *StructDecl =
          Result.Nodes.getNodeAs<clang::CXXRecordDecl>("struct_decl")) {
    if (!SM.isInMainFile(StructDecl->getLocation())) {
      return;
    }

    llvm::StringRef Filename = SM.getFilename(StructDecl->getLocation());
    if (!isTypesFile(Filename)) {
      return;
    }

    llvm::StringRef StructName = StructDecl->getName();
    const clang::DeclContext *DC = StructDecl->getDeclContext();
    std::string OperationName = extractOperationName(DC);

    if (OperationName.empty()) {
      diag(StructDecl->getLocation(),
           "generic type name '%0' should be renamed to an operation-specific "
           "name (e.g., '{Operation}Params' or '{Operation}Inputs')")
          << StructName;
      return;
    }

    std::string SuggestedName = getSuggestedName(StructName, OperationName);
    if (SuggestedName.empty()) {
      return;
    }

    // Get the location of just the struct name for the fix-it
    clang::SourceLocation NameLoc = StructDecl->getLocation();

    auto Diag = diag(NameLoc, "generic type name '%0' should be renamed to '%1'")
                << StructName << SuggestedName;

    // Add fix-it to rename the struct
    Diag << clang::FixItHint::CreateReplacement(
        clang::CharSourceRange::getTokenRange(NameLoc, NameLoc), SuggestedName);

    return;
  }

  // Handle type usages (update references)
  if (const auto *TL = Result.Nodes.getNodeAs<clang::TypeLoc>("type_loc")) {
    if (!SM.isInMainFile(TL->getBeginLoc())) {
      return;
    }

    // Skip types files - we only want to fix usages, not the definitions themselves
    llvm::StringRef Filename = SM.getFilename(TL->getBeginLoc());
    if (isTypesFile(Filename)) {
      return;
    }

    // Only handle specific TypeLoc kinds that represent actual type names in source
    // This avoids matching implied types, function types, etc.
    clang::TypeLoc InnerTL = *TL;

    // Try to get RecordTypeLoc or TypedefTypeLoc directly
    auto RecTL = InnerTL.getAs<clang::RecordTypeLoc>();
    if (!RecTL) {
      // Also check for elaborated types like "struct foo" or "class foo"
      if (auto ElabTL = InnerTL.getAs<clang::ElaboratedTypeLoc>()) {
        RecTL = ElabTL.getNamedTypeLoc().getAs<clang::RecordTypeLoc>();
      }
    }

    if (!RecTL) {
      return;
    }

    clang::QualType QT = RecTL.getType();
    if (QT.isNull()) {
      return;
    }

    // Get the fully qualified type string to check if it's a target type
    std::string FullTypeStr = QT.getCanonicalType().getAsString();

    if (!isTargetType(FullTypeStr)) {
      return;
    }

    // Extract operation name from the fully qualified type
    std::string OperationName = extractOperationFromFullyQualifiedType(FullTypeStr);
    if (OperationName.empty()) {
      return;
    }

    std::string TargetTypeName = getTargetTypeName(FullTypeStr);
    if (TargetTypeName.empty()) {
      return;
    }

    std::string SuggestedName = getSuggestedName(TargetTypeName, OperationName);
    if (SuggestedName.empty()) {
      return;
    }

    // Get the source range for just the type name
    clang::SourceRange Range = RecTL.getSourceRange();
    if (Range.isInvalid()) {
      return;
    }

    // For elaborated types (slice::operation_attributes_t), we want to replace
    // the whole thing including the namespace qualifier
    if (auto ElabTL = TL->getAs<clang::ElaboratedTypeLoc>()) {
      Range = ElabTL.getSourceRange();
    }

    // Get what's actually written in the source
    std::string WrittenTypeStr = QT.getAsString();

    auto Diag = diag(Range.getBegin(), "replace '%0' with '%1'")
                << WrittenTypeStr << SuggestedName;

    Diag << clang::FixItHint::CreateReplacement(Range, SuggestedName);
  }
}

} // namespace clang::tidy::ttnn
