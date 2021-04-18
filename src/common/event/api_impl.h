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

#include "src/common/event/api.h"

namespace px {
namespace event {

/**
 * APIImpl is the default implementation of the API.
 */
class APIImpl : public API {
 public:
  explicit APIImpl(TimeSystem* time_system) : time_system_(time_system) {}

  DispatcherUPtr AllocateDispatcher(std::string_view name) override;
  const event::TimeSource& TimeSourceRef() const override;

 private:
  // Unowned pointer to the time_system.
  TimeSystem* time_system_;
};

}  // namespace event
}  // namespace px
