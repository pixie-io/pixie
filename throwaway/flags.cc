#include <tbb/task_group.h>

#include <iostream>
#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/strings/str_format.h"
#include "src/common/common.h"

DEFINE_string(message, "", "The message to print");

int Fib(int n) {
  if (n < 2) {
    return n;
  }
  int x, y;
  tbb::task_group g;
  g.run([&] { x = Fib(n - 1); });  // spawn a task
  g.run([&] { y = Fib(n - 2); });  // spawn another task
  g.wait();                        // wait for both tasks to complete
  return x + y;
}

int main(int argc, char **argv) {
  absl::InitializeSymbolizer(argv[0]);
  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  LOG(INFO) << absl::StrFormat("Message: %s", FLAGS_message);
  std::cout << Fib(10) << std::endl;
  return 0;
}
