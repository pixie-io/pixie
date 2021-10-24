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

#pragma once

#include <memory>
#include <string>

#include "src/shared/stats/tags.h"

namespace pl {
namespace shared {
namespace stats {

/**
 * Metric is the base class for all tracked metrics.
 * It provides basic information such as name and tags.
 */
class Metric {
 public:
  virtual ~Metric() = default;

  /**
   * Return the name of the metric.
   */
  virtual std::string Name() const = 0;

  /**
   * Return the tags associated with the metric.
   */
  virtual const TagVector& Tags() const = 0;
};

/**
 * Counter is a metric used for monotonically increasing values.
 *
 * It has a max value of 2^64 - 1. The implementation should be thread safe.
 */
class Counter : public Metric {
 public:
  virtual ~Counter() = default;

  /**
   * Add the passed in delta to the current counter.
   */
  virtual void Add(uint64_t val) = 0;

  /**
   * Increment counter by one.
   */
  void Inc() { Add(1); }

  /**
   * Get the current value.
   */
  virtual uint64_t Value() = 0;
};

/**
 * Gauge is a metric used for tracking bounded values.
 *
 * It has a max value of 2^64 - 1. The implementation should be thread safe.
 */
class Gauge : public Metric {
 public:
  virtual ~Gauge() = default;

  /**
   * Set the value of the Gauge.
   */
  virtual void Set(uint64_t val) = 0;

  /**
   * Add the value to the Gauge.
   */
  virtual void Add(uint64_t val) = 0;

  /**
   * Subtract the value from the Gauge.
   */
  virtual void Sub(uint64_t val) = 0;

  /**
   * Get the current value of the Gauge
   */
  virtual uint64_t Value() = 0;
};

/**
 * Create a new counter with the given name and tags.
 */
std::shared_ptr<Counter> MakeCounter(std::string name, TagVector tags);

/**
 * Create a new gauge with the given name and tags.
 */
std::shared_ptr<Gauge> MakeGauge(std::string name, TagVector tags);

}  // namespace stats
}  // namespace shared
}  // namespace pl
