#pragma once

namespace pl {
namespace stirling {

// Provides a string view into a char array included in the binary via objcopy.
// Useful for include BPF programs that are copied into the binary.
#define OBJ_STRVIEW(varname, objname)     \
  extern char objname##_start;            \
  extern char objname##_end;              \
  inline const std::string_view varname = \
      std::string_view(&objname##_start, &objname##_end - &objname##_start);

// Macro to load BPF source code embedded in object files.
// See 'pl_bpf_cc_resource' bazel rule to see how these are generated.
#define BPF_SRC_STRVIEW(varname, build_label) OBJ_STRVIEW(varname, _binary_##build_label##_bpf_src);

// Define NO_OPT_ATTR that specifies that function should not be optimized away.
// Typically used on dummy probe triggers.
// Note that the attributes are different depending on the compiler.
#if defined(__clang__)
#define NO_OPT_ATTR __attribute__((noinline, optnone))
#elif defined(__GNUC__) || defined(__GNUG__)
#define NO_OPT_ATTR __attribute__((noinline, optimize("O0")))
#endif

}  // namespace stirling
}  // namespace pl
