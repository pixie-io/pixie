// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

// Need this line to call clone(). Otherwise the wrapper is not defined.
#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Used by proc_tracer_bpf_test.cc to test BPF probes of clone() and vfork().
//
// Forks 2 child processes that exit immediately. Prints PIDs of this process and those 2 child
// processes.

int do_something() { return 0; }

int main() {
  void* child_stack = malloc(16384);
  pid_t pid1 = clone(do_something, child_stack, /*flags*/ 0x0, NULL);
  pid_t pid2 = vfork();
  if (pid2 == 0) {
    _exit(0);
  }
  printf("%d %d %d\n", getpid(), pid1, pid2);
  return 0;
}
