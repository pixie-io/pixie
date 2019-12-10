// This executable is only for testing purposes, to see if we can find the function symbols.

// Using extern C to avoid name mangling (which just keeps the test a bit more readable).
extern "C" {
int CanYouFindThis(int a, int b) { return a + b; }
}

int main() {
  return CanYouFindThis(3, 4);
}
