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
#include <atomic>
#include <utility>

#include "src/shared/stats/metrics.h"

namespace pl {
namespace shared {
namespace stats {

class AtomicCounter : public Counter {
 public:
  AtomicCounter(std::string name, TagVector tags)
      : name_(std::move(name)), tags_(std::move(tags)), value_(0) {}

  ~AtomicCounter() override = default;

  std::string Name() const override { return name_; }

  const TagVector& Tags() const override { return tags_; }

  void Add(uint64_t val) override { value_ += val; }

  uint64_t Value() override { return value_.load(); }

 private:
  std::string name_;
  TagVector tags_;
  std::atomic<std::uint64_t> value_;
};

class AtomicGauge : public Gauge {
 public:
  AtomicGauge(std::string name, TagVector tags)
      : name_(std::move(name)), tags_(std::move(tags)), value_(0) {}

  ~AtomicGauge() override = default;

  std::string Name() const override { return name_; }

  const TagVector& Tags() const override { return tags_; }

  void Set(uint64_t val) override { value_.store(val); }

  void Add(uint64_t val) override { value_ += val; }

  void Sub(uint64_t val) override { value_ -= val; }

  uint64_t Value() override { return value_.load(); }

 private:
  std::string name_;
  TagVector tags_;
  std::atomic<std::uint64_t> value_;
};

std::shared_ptr<Counter> MakeCounter(std::string name, TagVector tags) {
  return std::make_shared<AtomicCounter>(std::move(name), std::move(tags));
}
std::shared_ptr<Gauge> MakeGauge(std::string name, TagVector tags) {
  return std::make_shared<AtomicGauge>(std::move(name), std::move(tags));
}

}  // namespace stats
}  // namespace shared
}  // namespace pl
