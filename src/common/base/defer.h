#pragma once

#include <functional>
#include <utility>

#include "src/common/base/macros.h"
#include "src/common/base/mixins.h"

namespace px {

// Defer allow you to defer code (go-style)
//
// Usage: DEFER({ statement1(); statement2(); });
//
// You can write code like the following:
//
//   int fd = open(file, O_RDONLY);
//   DEFER(close(orig_net_ns_fd););
//   ...
//   if (some_condition) {
//     return;
//   }
//   ...
//   return;
//
// NOTE: Unlike Golang, this DEFER runs at the end of its scope, not at the end of the function.

template <typename FnType>
class ScopedLambda : public NotCopyable {
 public:
  explicit ScopedLambda(FnType fn) : fn_(std::move(fn)) {}
  ~ScopedLambda() { fn_(); }

 private:
  FnType fn_;
};

template <typename FnType>
ScopedLambda<FnType> MakeScopedLambda(FnType fn) {
  return ScopedLambda<FnType>(std::move(fn));
}

}  // namespace px

#define DEFER(...) auto PL_UNIQUE_NAME(varname) = px::MakeScopedLambda([&] { __VA_ARGS__; });
