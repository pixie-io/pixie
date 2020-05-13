// This executable is only for testing purposes.
// We use it to see if we can find the function symbols and debug information.

struct PairStruct {
  int a;
  int b;
};

// Using extern C to avoid name mangling (which just keeps the test a bit more readable).
extern "C" {
int CanYouFindThis(int a, int b) { return a + b; }
int SomeFunction(PairStruct x) { return x.a + x.b; }
}

int main() {
  return CanYouFindThis(3, 4);
}
