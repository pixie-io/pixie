#pragma once
/**
 * This file exports all the common test libraries so that we
 * don't need to manually import them everywhere.
 */

#include <gmock/gmock.h>  // IWYU pragma: export
#include <gtest/gtest.h>  // IWYU pragma: export

#include "src/common/base/test_utils.h"           // IWYU pragma: export
#include "src/common/testing/line_diff.h"         // IWYU pragma: export
#include "src/common/testing/protobuf.h"          // IWYU pragma: export
#include "src/common/testing/status.h"            // IWYU pragma: export
#include "src/common/testing/temp_dir.h"          // IWYU pragma: export
#include "src/common/testing/test_environment.h"  // IWYU pragma: export
