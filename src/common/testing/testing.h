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
/**
 * This file exports all the common test libraries so that we
 * don't need to manually import them everywhere.
 */

#include <gmock/gmock.h>  // IWYU pragma: export
#include <gtest/gtest.h>  // IWYU pragma: export

#include "src/common/testing/line_diff.h"         // IWYU pragma: export
#include "src/common/testing/matchers.h"          // IWYU pragma: export
#include "src/common/testing/protobuf.h"          // IWYU pragma: export
#include "src/common/testing/status.h"            // IWYU pragma: export
#include "src/common/testing/temp_dir.h"          // IWYU pragma: export
#include "src/common/testing/test_environment.h"  // IWYU pragma: export
#include "src/common/testing/testing.h"           // IWYU pragma: export
