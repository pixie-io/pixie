#include "src/common/base/env.h"

#include <mutex>  // NOLINT
#include "absl/debugging/symbolize.h"

namespace pl {

std::once_flag init_once, shutdown_once;

void InitEnvironmentOrDieImpl(int* argc, char** argv) {
  // Enable logging by default.
  FLAGS_logtostderr = true;
  FLAGS_colorlogtostderr = true;

  absl::InitializeSymbolizer(argv[0]);
  google::ParseCommandLineFlags(argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "Started: " << argv[0];
}

void ShutdownEnvironmentOrDieImpl() {
  LOG(INFO) << "Shutdown: ";
  google::ShutdownGoogleLogging();
}

void InitEnvironmentOrDie(int* argc, char** argv) {
  CHECK(argc != nullptr) << "argc must not be null";
  CHECK(argv != nullptr) << "argv must not be null";
  std::call_once(init_once, InitEnvironmentOrDieImpl, argc, argv);
}

void ShutdownEnvironmentOrDie() { std::call_once(shutdown_once, ShutdownEnvironmentOrDieImpl); }

}  // namespace pl
