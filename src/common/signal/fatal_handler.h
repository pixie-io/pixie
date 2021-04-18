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

namespace px {

/**
 * FatalErrorHandler is the interface to use for error handling
 * for fatal errors.
 *
 * The behavior may not be well defined if the functions are not
 * async-signal-safe.
 */
class FatalErrorHandlerInterface {
 public:
  virtual ~FatalErrorHandlerInterface() = default;
  virtual void OnFatalError() const = 0;
};

}  // namespace px
