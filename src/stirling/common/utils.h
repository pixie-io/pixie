#pragma once

#include "src/common/base/utils.h"

namespace pl {
namespace stirling {

struct TimeSpan {
  uint64_t begin_ns = 0;
  uint64_t end_ns = 0;
};

}  // namespace stirling
}  // namespace pl

#define BCC_SRC_STRVIEW(varname, build_label) OBJ_STRVIEW(varname, _binary_##build_label##_bpf_src);
