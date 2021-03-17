#include <signal.h>
#include <stdint.h>

#include <iostream>

uint64_t fib(const uint64_t n) {
  if (n == 0) {
    return 0;
  }
  if (n == 1) {
    return 1;
  }

  const int64_t numIters = n - 2;
  uint64_t prev = 1;
  uint64_t curr = 1;
  uint64_t next = 1;

  for (int64_t i = 0; i < numIters; ++i) {
    next = prev + curr;
    prev = curr;
    curr = next;
  }
  return next;
}

uint64_t fib52() { return fib(52); }

uint64_t fib27() { return fib(27); }

bool run = true;

void SignalHandler(int /*s*/) { run = false; }

int main() {
  signal(SIGINT, SignalHandler);

  uint64_t f52 = 0;
  uint64_t f27 = 0;

  while (run) {
    // Calling fib(N) performs N-2 iterations.
    // fib52() & fib27() are "named" so that their
    // symbols are distinct in the stack traces.
    // The stack trace histo should be approx.:
    // fib27(): 33% of observations
    // fib52(): 66% of observations
    f52 = fib52();
    f27 = fib27();
  }

  std::cout << std::endl;
  std::cout << "F27: " << f27 << std::endl;
  std::cout << "F52: " << f52 << std::endl;
  return 0;
}
