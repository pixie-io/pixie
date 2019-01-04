#include "src/common/env.h"

#include <mutex>  // NOLINT
#include <string>

namespace pl {

std::once_flag init_once, shutdown_once;

void InitEnvironmentOrDieImpl(int *argc, char **argv) {
  google::ParseCommandLineFlags(argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  LOG(INFO) << "Started: " << argv[0];
}

void ShutdownEnvironmentOrDieImpl() {
  LOG(INFO) << "Shutdown: ";
  google::ShutdownGoogleLogging();
}

void InitEnvironmentOrDie(int *argc, char **argv) {
  CHECK(argc != nullptr) << "argc must not be null";
  CHECK(argv != nullptr) << "argv must not be null";
  std::call_once(init_once, InitEnvironmentOrDieImpl, argc, argv);
}

void ShutdownEnvironmentOrDie() { std::call_once(shutdown_once, ShutdownEnvironmentOrDieImpl); }

}  // namespace pl
