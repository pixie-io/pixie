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

#include <string>

#include "src/common/base/statusor.h"

namespace px {
namespace zlib {

/**
 * @brief Inflates (gunzip) a source buffer and returns the decompressed content as a string.
 *
 * @param in A view into the source buffer.
 * @param output_block_size How many bytes to decompress into the output buffer at a time.
 *        For small strings, best to keep this only slightly larger than the expected output size.
 * @return Status or the decompressed content as a string.
 */
StatusOr<std::string> Inflate(std::string_view in, size_t output_block_size = 16384);

}  // namespace zlib
}  // namespace px
