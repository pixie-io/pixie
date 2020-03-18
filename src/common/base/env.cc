#include "src/common/base/env.h"

#include <absl/debugging/symbolize.h>
#include <absl/strings/substitute.h>
#include <cstdlib>
#include <filesystem>
#include <mutex>  // NOLINT

namespace pl {

namespace {

std::once_flag init_once, shutdown_once;

void InitEnvironmentOrDieImpl(int* argc, char** argv) {
  // Enable logging by default.
  FLAGS_logtostderr = true;
  FLAGS_colorlogtostderr = true;

  absl::InitializeSymbolizer(argv[0]);
  google::ParseCommandLineFlags(argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  ChDirPixieRoot();

  LOG(INFO) << "Started: " << argv[0];
}

void ShutdownEnvironmentOrDieImpl() {
  LOG(INFO) << "Shutting down";
  google::ShutdownGoogleLogging();
}

void InitEnvironmentOrDie(int* argc, char** argv) {
  CHECK(argc != nullptr) << "argc must not be null";
  CHECK(argv != nullptr) << "argv must not be null";
  std::call_once(init_once, InitEnvironmentOrDieImpl, argc, argv);
}

void ShutdownEnvironmentOrDie() { std::call_once(shutdown_once, ShutdownEnvironmentOrDieImpl); }

}  // namespace

EnvironmentGuard::EnvironmentGuard(int* argc, char** argv) { InitEnvironmentOrDie(argc, argv); }
EnvironmentGuard::~EnvironmentGuard() { ShutdownEnvironmentOrDie(); }

std::optional<std::string> GetEnv(const std::string& env_var) {
  const char* var = getenv(env_var.c_str());
  if (var == nullptr) {
    return std::nullopt;
  }
  return std::string(var);
}

void ChDirPixieRoot() {
  // If PIXIE_ROOT is set, and we are not running through bazel, run from ToT.
  const char* test_src_dir = std::getenv("TEST_SRCDIR");
  const char* pixie_root = std::getenv("PIXIE_ROOT");
  if (test_src_dir == nullptr && pixie_root != nullptr) {
    LOG(INFO) << absl::Substitute("Changing CWD to to PIXIE_ROOT [$0]", pixie_root);
    std::filesystem::current_path(pixie_root);
  }
}

}  // namespace pl
