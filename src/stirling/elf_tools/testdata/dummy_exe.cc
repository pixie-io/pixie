
// This function is here only for testing purposes.
// Using extern C to avoid name mangling (which just keeps the test a bit more readable).
extern "C" {
int CanYouFindThis(int a, int b) { return a + b; }
}

int main() {
  return CanYouFindThis(3, 4);
}
