/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/common/base/env.h"

#include <absl/debugging/symbolize.h>
#include <absl/strings/str_format.h>
#include <absl/strings/substitute.h>
#include <cstdlib>
#include <filesystem>
#include <mutex>  // NOLINT

namespace px {

namespace {

std::once_flag init_once, shutdown_once;

void InitEnvironmentOrDieImpl(int* argc, char** argv) {
  // Enable logging by default.
  FLAGS_logtostderr = true;
  FLAGS_colorlogtostderr = true;

  std::string cmd = absl::StrJoin(argv, argv + *argc, " ");

  absl::InitializeSymbolizer(argv[0]);
  google::ParseCommandLineFlags(argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  ChDirPixieRoot();

  LOG(INFO) << "Started: " << cmd;
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

void ProcessStatsMonitor::Reset() {
  start_time_ = std::chrono::steady_clock::now();
  getrusage(RUSAGE_SELF, &start_usage_);
}

namespace {
double TimevalToNanoseconds(const struct timeval& x) { return x.tv_sec * 1e9 + x.tv_usec * 1e3; }
}  // namespace

void ProcessStatsMonitor::PrintCPUTime() {
  std::chrono::time_point<std::chrono::steady_clock> stop_time = std::chrono::steady_clock::now();
  std::chrono::nanoseconds run_time = stop_time - start_time_;

  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  double end_utime_ns = TimevalToNanoseconds(usage.ru_utime);
  double start_utime_ns = TimevalToNanoseconds(start_usage_.ru_utime);
  double utime_ns = end_utime_ns - start_utime_ns;

  double end_stime_ns = TimevalToNanoseconds(usage.ru_stime);
  double start_stime_ns = TimevalToNanoseconds(start_usage_.ru_stime);
  double stime_ns = end_stime_ns - start_stime_ns;

  LOG(INFO) << absl::StrFormat("CPU usage: %0.1f%% user, %0.1f%% system, %0.1f%% total",
                               100 * utime_ns / run_time.count(), 100 * stime_ns / run_time.count(),
                               100 * (utime_ns + stime_ns) / run_time.count());
}

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

}  // namespace px
