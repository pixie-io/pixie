#pragma once

#include <unistd.h>

#include <cstring>

namespace pl {

inline bool IsRoot() { return (geteuid() == 0); }

/**
 * @brief Copy from BPF data pointed by a pointer. The target should be a memory aligned type to
 * necessitate this function. This is because BPF memory might lost memory alignment when copied
 * from BPF to perf buffer, in which case, a naive pointer cast might cause runtime error in ASAN,
 * or in rare situations where the compiler produces misaligned code on different CPU arches.
 */
template <typename MemAlignedType>
inline MemAlignedType CopyFromBPF(const void* data) {
  MemAlignedType result;
  memcpy(&result, data, sizeof(MemAlignedType));
  return result;
}

}  // namespace pl

// Provides a string view into a char array included in the binary via objcopy.
// Useful for include BPF programs that are copied into the binary.
#define OBJ_STRVIEW(varname, objname)     \
  extern char objname##_start;            \
  extern char objname##_end;              \
  inline const std::string_view varname = \
      std::string_view(&objname##_start, &objname##_end - &objname##_start);
