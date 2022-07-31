/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// This executable is only for testing purposes.
// We use it to see if we can find the function symbols and debug information.

#include <unistd.h>
#include <iostream>

struct ABCStruct32 {
  int32_t a;
  int32_t b;
  int32_t c;
};

struct ABCStruct64 {
  int64_t a;
  int64_t b;
  int64_t c;
};

struct LowerStruct {
  bool L0;
  int32_t L1;
  int64_t* L2;
};

struct MidStruct {
  LowerStruct M0;
  bool M1;
  LowerStruct M2;
};

struct OuterStruct {
  int64_t O0;
  MidStruct O1;
};

// Using extern C to avoid name mangling (which just keeps the test a bit more readable).
extern "C" {
int CanYouFindThis(int a, int b) { return a + b; }

ABCStruct32 ABCSum32(ABCStruct32 x, ABCStruct32 y) {
  return ABCStruct32{x.a + y.a, x.b + y.b, x.c + y.c};
}

ABCStruct64 ABCSum64(ABCStruct64 x, ABCStruct64 y) {
  return ABCStruct64{x.a + y.a, x.b + y.b, x.c + y.c};
}

// x is passed via 2 registers.
// y is passed by memory/stack, because it is too large.
// z_a, z_b, z_c are passed by a register each.
// w is passed by memory/stack, because no more registers are available.
ABCStruct64 ABCSumMixed(ABCStruct32 x, ABCStruct64 y, int32_t z_a, int64_t z_b, int32_t z_c,
                        ABCStruct32 w) {
  return ABCStruct64{x.a + y.a + z_a + w.a, x.b + y.b + z_b + w.b, x.c + y.c + z_c + w.c};
}

void SomeFunctionWithPointerArgs(int* a, ABCStruct32* x) {
  x->a = *a;
  a++;
}

}  // extern "C"

namespace px {
namespace testing {

class Foo {
 public:
  int Bar(int i) const { return i * i; }
};

}  // namespace testing
}  // namespace px

int main() {
  OuterStruct x;
  (void)(x);

  for (int i = 0; i < 10; ++i) {
    int sum = CanYouFindThis(3, 4);
    std::cout << sum << std::endl;

    ABCStruct32 abc_struct_32_sum = ABCSum32(ABCStruct32{1, 2, 3}, ABCStruct32{4, 5, 6});
    std::cout << abc_struct_32_sum.a << std::endl;

    ABCStruct64 abc_struct_64_sum = ABCSum64(ABCStruct64{1, 2, 3}, ABCStruct64{4, 5, 6});
    std::cout << abc_struct_64_sum.a << std::endl;

    ABCStruct64 abc_mixed_sum = ABCSumMixed(ABCStruct32{1, 2, 3}, ABCStruct64{4, 5, 6}, 7, 8, 9,
                                            ABCStruct32{0xa, 0xb0, 0xc});
    std::cout << abc_mixed_sum.a << std::endl;

    px::testing::Foo foo;
    std::cout << foo.Bar(3) << std::endl;

    sleep(1);
  }

  return 0;
}
