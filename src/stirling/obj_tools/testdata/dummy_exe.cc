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

// Using extern C to avoid name mangling (which just keeps the test a bit more readable).
extern "C" {
int CanYouFindThis(int a, int b) { return a + b; }

ABCStruct32 ABCSum32(ABCStruct32 x, ABCStruct32 y) { return ABCStruct32{x.a+y.a, x.b+y.b, x.c+y.c}; }

ABCStruct64 ABCSum64(ABCStruct64 x, ABCStruct64 y) { return ABCStruct64{x.a+y.a, x.b+y.b, x.c+y.c}; }

void SomeFunctionWithPointerArgs(int* a, ABCStruct32* x) { x->a = *a; a++; }
}

namespace pl {
namespace testing {

class Foo {
 public:
  int Bar(int i) const {
    return i * i;
  }
};

}  // namespace testing
}  // namespace pl

int main() {
  while (true) {
    int sum = CanYouFindThis(3, 4);
    std::cout << sum << std::endl;

    ABCStruct32 abc_struct_32_sum = ABCSum32(ABCStruct32{1, 2, 3}, ABCStruct32{4, 5, 6});
    std::cout << abc_struct_32_sum.a << std::endl;

    ABCStruct64 abc_struct_64_sum = ABCSum64(ABCStruct64{1, 2, 3}, ABCStruct64{4, 5, 6});
    std::cout << abc_struct_64_sum.a << std::endl;

    pl::testing::Foo foo;
    std::cout << foo.Bar(3) << std::endl;

    sleep(1);
  }

  return 0;
}
