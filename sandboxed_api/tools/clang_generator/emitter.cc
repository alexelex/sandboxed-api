// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "sandboxed_api/tools/clang_generator/emitter.h"

#include "absl/random/random.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/strip.h"
#include "clang/AST/DeclCXX.h"
#include "sandboxed_api/tools/clang_generator/diagnostics.h"
#include "sandboxed_api/util/status_macros.h"

namespace sapi {

// Common file prolog with auto-generation notice.
// Note: The includes will be adjusted by Copybara when converting to/from
//       internal code. This is intentional.
// Text template arguments:
//   1. Header guard
constexpr absl::string_view kHeaderProlog =
    R"(// AUTO-GENERATED by the Sandboxed API generator.
// Edits will be discarded when regenerating this file.

#ifndef %1$s
#define %1$s

#include <cstdint>
#include <type_traits>

#include "absl/base/macros.h"
#include "absl/status/status.h"
#include "sandboxed_api/util/statusor.h"
#include "sandboxed_api/sandbox.h"
#include "sandboxed_api/vars.h"
#include "sandboxed_api/util/status_macros.h"

)";
constexpr absl::string_view kHeaderEpilog =
    R"(
#endif  // %1$s)";

// Text template arguments:
//   1. Include for embedded sandboxee objects
constexpr absl::string_view kEmbedInclude = R"(#include "%1$s_embed.h"

)";

// Text template arguments:
//   1. Namespace name
constexpr absl::string_view kNamespaceBeginTemplate =
    R"(
namespace %1$s {

)";
constexpr absl::string_view kNamespaceEndTemplate =
    R"(
}  // namespace %1$s
)";

// Text template arguments:
//   1. Class name
//   2. Embedded object identifier
constexpr absl::string_view kEmbedClassTemplate = R"(
// Sandbox with embedded sandboxee and default policy
class %1$s : public ::sapi::Sandbox {
 public:
  %1$s() : ::sapi::Sandbox(%2$s_embed_create()) {}
};

)";

// Text template arguments:
//   1. Class name
constexpr absl::string_view kClassHeaderTemplate = R"(
// Sandboxed API
class %1$s {
 public:
  explicit %1$s(::sapi::Sandbox* sandbox) : sandbox_(sandbox) {}

  ABSL_DEPRECATED("Call sandbox() instead")
  ::sapi::Sandbox* GetSandbox() const { return sandbox(); }
  ::sapi::Sandbox* sandbox() const { return sandbox_; }
)";

constexpr absl::string_view kClassFooterTemplate = R"(
 private:
  ::sapi::Sandbox* sandbox_;
};
)";

std::string GetIncludeGuard(absl::string_view filename) {
  if (filename.empty()) {
    static auto* bit_gen = new absl::BitGen();
    return absl::StrCat(
        // Copybara will transform the string. This is intentional.
        "SANDBOXED_API_GENERATED_HEADER_",
        absl::AsciiStrToUpper(absl::StrCat(
            absl::Hex(absl::Uniform<uint64_t>(*bit_gen), absl::kZeroPad16))),
        "_");
  }

  constexpr absl::string_view kUnderscorePrefix = "SAPI_";
  std::string guard;
  guard.reserve(filename.size() + kUnderscorePrefix.size() + 1);
  for (auto c : filename) {
    if (absl::ascii_isalpha(c)) {
      guard += absl::ascii_toupper(c);
      continue;
    }
    if (guard.empty()) {
      guard = kUnderscorePrefix;
    }
    if (absl::ascii_isdigit(c)) {
      guard += c;
    } else if (guard.back() != '_') {
      guard += '_';
    }
  }
  if (!absl::EndsWith(guard, "_")) {
    guard += '_';
  }
  return guard;
}

// Returns the namespace components of a declaration's qualified name.
std::vector<std::string> GetNamespacePath(const clang::TypeDecl* decl) {
  std::vector<std::string> comps;
  for (const auto* ctx = decl->getDeclContext(); ctx; ctx = ctx->getParent()) {
    if (const auto* nd = llvm::dyn_cast<clang::NamespaceDecl>(ctx)) {
      comps.push_back(nd->getNameAsString());
    }
  }
  std::reverse(comps.begin(), comps.end());
  return comps;
}

// Serializes the given Clang AST declaration back into compilable source code.
std::string PrintAstDecl(const clang::Decl* decl) {
  // TODO(cblichmann): Make types nicer
  //   - Rewrite typedef to using
  //   - Rewrite function pointers using std::add_pointer_t<>;

  if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
    // For C++ classes/structs, only emit a forward declaration.
    return absl::StrCat(record->isClass() ? "class " : "struct ",
                        std::string(record->getName()));
  }
  std::string pretty;
  llvm::raw_string_ostream os(pretty);
  decl->print(os);
  return os.str();
}

std::string GetParamName(const clang::ParmVarDecl* decl, int index) {
  if (std::string name = decl->getName().str(); !name.empty()) {
    return absl::StrCat(name, "_");  // Suffix to avoid collisions
  }
  return absl::StrCat("unnamed", index, "_");
}

std::string PrintFunctionPrototype(const clang::FunctionDecl* decl) {
  // TODO(cblichmann): Fix function pointers and anonymous namespace formatting
  std::string out =
      absl::StrCat(decl->getDeclaredReturnType().getAsString(), " ",
                   std::string(decl->getQualifiedNameAsString()), "(");

  std::string print_separator;
  for (int i = 0; i < decl->getNumParams(); ++i) {
    const clang::ParmVarDecl* param = decl->getParamDecl(i);

    absl::StrAppend(&out, print_separator);
    print_separator = ", ";
    absl::StrAppend(&out, param->getType().getAsString());
    if (std::string name = param->getName().str(); !name.empty()) {
      absl::StrAppend(&out, " ", name);
    }
  }
  absl::StrAppend(&out, ")");
  return out;
}

sapi::StatusOr<std::string> EmitFunction(const clang::FunctionDecl* decl) {
  std::string out;
  absl::StrAppend(&out, "\n// ", PrintFunctionPrototype(decl), "\n");
  const std::string function_name = decl->getNameAsString();
  const clang::QualType return_type = decl->getDeclaredReturnType();
  const bool returns_void = return_type->isVoidType();

  const clang::ASTContext& context = decl->getASTContext();

  // "Status<OptionalReturn> FunctionName("
  absl::StrAppend(&out, MapQualTypeReturn(context, return_type), " ",
                  function_name, "(");

  struct ParameterInfo {
    clang::QualType qual;
    std::string name;
  };
  std::vector<ParameterInfo> params;

  std::string print_separator;
  for (int i = 0; i < decl->getNumParams(); ++i) {
    const clang::ParmVarDecl* param = decl->getParamDecl(i);

    ParameterInfo& param_info = params.emplace_back();
    param_info.qual = param->getType();
    param_info.name = GetParamName(param, i);

    absl::StrAppend(&out, print_separator);
    print_separator = ", ";
    absl::StrAppend(&out, MapQualTypeParameter(context, param_info.qual), " ",
                    param_info.name);
  }

  absl::StrAppend(&out, ") {\n");
  absl::StrAppend(&out, MapQualType(context, return_type), " v_ret_;\n");
  for (const auto& [qual, name] : params) {
    if (!IsPointerOrReference(qual)) {
      absl::StrAppend(&out, MapQualType(context, qual), " v_", name, "(", name,
                      ");\n");
    }
  }
  absl::StrAppend(&out, "\nSAPI_RETURN_IF_ERROR(sandbox_->Call(\"", function_name,
                  "\", &v_ret_");
  for (const auto& [qual, name] : params) {
    absl::StrAppend(&out, ", ", IsPointerOrReference(qual) ? "" : "&v_", name);
  }
  absl::StrAppend(&out, "));\nreturn ",
                  (returns_void ? "absl::OkStatus()" : "v_ret_.GetValue()"),
                  ";\n}\n");
  return out;
}

sapi::StatusOr<std::string> EmitHeader(
    std::vector<clang::FunctionDecl*> functions, const QualTypeSet& types,
    const GeneratorOptions& options) {
  std::string out;
  const std::string include_guard = GetIncludeGuard(options.out_file);
  absl::StrAppendFormat(&out, kHeaderProlog, include_guard);

  // When embedding the sandboxee, add embed header include
  if (!options.embed_name.empty()) {
    // Not using JoinPath() because even on Windows include paths use plain
    // slashes.
    std::string include_file(absl::StripSuffix(
        absl::StrReplaceAll(options.embed_dir, {{"\\", "/"}}), "/"));
    if (!include_file.empty()) {
      absl::StrAppend(&include_file, "/");
    }
    absl::StrAppend(&include_file, options.embed_name);
    absl::StrAppendFormat(&out, kEmbedInclude, include_file);
  }

  // If specified, wrap the generated API in a namespace
  if (options.has_namespace()) {
    absl::StrAppendFormat(&out, kNamespaceBeginTemplate,
                          options.namespace_name);
  }

  // Emit type dependencies
  // TODO(cblichmann): Coalesce namespaces
  std::string out_types = "// Types this API depends on\n";
  bool added_types = false;
  for (const clang::QualType& qual : types) {
    clang::TypeDecl* decl = nullptr;
    if (const auto* typedef_type = qual->getAs<clang::TypedefType>()) {
      decl = typedef_type->getDecl();
    } else if (const auto* enum_type = qual->getAs<clang::EnumType>()) {
      decl = enum_type->getDecl();
    } else {
      decl = qual->getAsRecordDecl();
    }
    if (!decl) {
      continue;
    }

    const std::vector<std::string> ns_path = GetNamespacePath(decl);
    std::string nested_ns_name;
    if (!ns_path.empty()) {
      if (const auto& ns_root = ns_path.front();
          ns_root == "std" || ns_root == "sapi" || ns_root == "__gnu_cxx") {
        // Filter out any and all declarations from the C++ standard library,
        // from SAPI itself and from other well-known namespaces. This avoids
        // re-declaring things like standard integer types, for example.
        continue;
      }
      nested_ns_name = absl::StrCat(ns_path[0].empty() ? "" : " ",
                                    absl::StrJoin(ns_path, "::"));
      absl::StrAppend(&out_types, "namespace", nested_ns_name, " {\n");
    }
    absl::StrAppend(&out_types, PrintAstDecl(decl), ";");
    if (!ns_path.empty()) {
      absl::StrAppend(&out_types, "\n}  // namespace", nested_ns_name);
    }
    absl::StrAppend(&out_types, "\n");
    added_types = true;
  }
  if (added_types) {
    absl::StrAppend(&out, out_types);
  }

  // Optionally emit a default sandbox that instantiates an embedded sandboxee
  if (!options.embed_name.empty()) {
    // TODO(cblichmann): Make the "Sandbox" suffix configurable.
    absl::StrAppendFormat(
        &out, kEmbedClassTemplate, absl::StrCat(options.name, "Sandbox"),
        absl::StrReplaceAll(options.embed_name, {{"-", "_"}}));
  }

  // Emit the actual Sandboxed API
  // TODO(cblichmann): Make the "Api" suffix configurable or at least optional.
  absl::StrAppendFormat(&out, kClassHeaderTemplate,
                        absl::StrCat(options.name, "Api"));
  std::string out_func;
  for (const clang::FunctionDecl* decl : functions) {
    SAPI_ASSIGN_OR_RETURN(out_func, EmitFunction(decl));
    absl::StrAppend(&out, out_func);
  }
  absl::StrAppend(&out, kClassFooterTemplate);

  // Close out the header: close namespace (if needed) and end include guard
  if (options.has_namespace()) {
    absl::StrAppendFormat(&out, kNamespaceEndTemplate, options.namespace_name);
  }
  absl::StrAppendFormat(&out, kHeaderEpilog, include_guard);
  return out;
}

}  // namespace sapi
