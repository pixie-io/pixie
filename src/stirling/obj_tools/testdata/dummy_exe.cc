// This executable is only for testing purposes.
// We use it to see if we can find the function symbols and debug information.

#include <iostream>

struct PairStruct {
  int a;
  int b;
  int c;
};

// Using extern C to avoid name mangling (which just keeps the test a bit more readable).
extern "C" {
int CanYouFindThis(int a, int b) { return a + b; }
PairStruct SomeFunction(PairStruct x, PairStruct y) { return PairStruct{x.a+y.a, x.b+y.b, x.c+y.c}; }
void SomeFunctionWithPointerArgs(int* a, PairStruct* x) { x->a = *a; a++; }
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
  CanYouFindThis(3, 4);

  pl::testing::Foo foo;
  std::cout << foo.Bar(3) << std::endl;

  return 0;
}
