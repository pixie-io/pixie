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
