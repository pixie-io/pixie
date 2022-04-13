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
#include <utility>

#include "src/common/system/config.h"

#include <unistd.h>

#include "src/common/base/base.h"
#include "src/common/clock/clock_conversion.h"
#include "src/common/fs/fs_wrapper.h"

namespace px {
namespace system {
using clock::ClockConverter;
using clock::DefaultMonoToRealtimeConverter;

DEFINE_string(sysfs_path, gflags::StringFromEnv("PL_SYSFS_PATH", "/sys/fs"),
              "The path to the sysfs directory.");

DEFINE_string(host_path, gflags::StringFromEnv("PL_HOST_PATH", ""),
              "The path to the host root directory.");

#include <ctime>

class ConfigImpl final : public Config {
 public:
  ConfigImpl(std::unique_ptr<ClockConverter> clock_converter)
      : host_path_(FLAGS_host_path),
        sysfs_path_(FLAGS_sysfs_path),
        proc_path_(absl::StrCat(FLAGS_host_path, "/proc")),
        clock_converter_(std::move(clock_converter)) {}

  bool HasConfig() const override { return true; }

  int64_t PageSizeBytes() const override { return sysconf(_SC_PAGESIZE); }

  int64_t KernelTicksPerSecond() const override { return sysconf(_SC_CLK_TCK); }

  int64_t KernelTickTimeNS() const override {
    return static_cast<int64_t>(1E9 / KernelTicksPerSecond());
  }

  uint64_t ConvertToRealTime(uint64_t monotonic_time) const override {
    return clock_converter_->Convert(monotonic_time);
  }

  const std::filesystem::path& sysfs_path() const override { return sysfs_path_; }

  const std::filesystem::path& host_path() const override { return host_path_; }

  const std::filesystem::path& proc_path() const override { return proc_path_; }

  std::filesystem::path ToHostPath(const std::filesystem::path& p) const override {
    // If we're running in a container, convert path to be relative to our host mount.
    // Note that we mount host '/' to '/host' inside container.
    // Warning: must use JoinPath, because we are dealing with two absolute paths.
    return fs::JoinPath({&host_path_, &p});
  }

  clock::ClockConverter* clock_converter() const override { return clock_converter_.get(); }

 private:
  const std::filesystem::path host_path_;
  const std::filesystem::path sysfs_path_;
  const std::filesystem::path proc_path_;
  std::unique_ptr<ClockConverter> clock_converter_;
};

namespace {
std::unique_ptr<ConfigImpl> g_instance;
}

const Config& Config::GetInstance() {
  if (g_instance == nullptr) {
    ResetInstance(std::make_unique<DefaultMonoToRealtimeConverter>());
  }
  return *g_instance;
}

void Config::ResetInstance(std::unique_ptr<ClockConverter> converter) {
  g_instance = std::make_unique<ConfigImpl>(std::move(converter));
}
void Config::ResetInstance() {
  Config::ResetInstance(std::make_unique<DefaultMonoToRealtimeConverter>());
}

}  // namespace system
}  // namespace px
