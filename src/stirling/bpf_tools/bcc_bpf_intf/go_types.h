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

struct gostring {
  const char* ptr;
  int64_t len;
};

struct go_byte_array {
  const char* ptr;
  int64_t len;
  int64_t cap;
};

struct go_int32_array {
  const int32_t* ptr;
  int64_t len;
  int64_t cap;
};

struct go_ptr_array {
  const void* ptr;
  int64_t len;
  int64_t cap;
};

struct go_interface {
  int64_t type;
  void* ptr;
};
