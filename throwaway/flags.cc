#include <gflags/gflags.h>
#include <glog/logging.h>

#include <iostream>
#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/strings/str_format.h"

DEFINE_string(message, "", "The message to print");

int main(int argc, char **argv) {
  absl::InitializeSymbolizer(argv[0]);
  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  LOG(INFO) << absl::StrFormat("Message: %s", FLAGS_message);
  return 0;
}
