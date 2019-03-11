#pragma once

#include <utility>

/**
 * A Constant Compile-Time String
 *
 * Inspired from https://gist.github.com/creative-quant/6aa863e1cb415cbb9056f3d86f23b2c4
 */
class ConstString {
 private:
  const char* const p_;

 public:
  template <std::size_t N>
  // NOLINTNEXTLINE: implicit constructor.
  constexpr ConstString(const char (&a)[N]) : p_(a) {}
  constexpr const char* get() const { return p_; }
};
