#pragma once

#include <string>

#include "src/common/base/statusor.h"

namespace pl {
namespace zlib {

/**
 * @brief Inflates (gunzip) a source buffer to a destination buffer.
 *
 * @param src Pointer to the source (compressed input) buffer.
 * @param src_len Size of the source buffer.
 * @param dst Pointer to the destination (decompressed output) buffer.
 * @param dst_len Size of the destination buffer.
 * @return zlib status (see zlib.h).
 */
int Inflate(const char *src, int src_len, char *dst, int dst_len);

/**
 * @brief Inflates (gunzip) a source buffer and returns the decompressed content as a string.
 *
 * @param str A view into the source buffer.
 * @param output_block_size How many bytes to decompress into the output buffer at a time.
 *        For small strings, best to keep this only slightly larger than the expected output size.
 * @return Status or the decompressed content as a string.
 */
StatusOr<std::string> StrInflate(std::string_view str, size_t output_block_size);

/**
 * @brief Inflates (gunzip) a source buffer and returns the decompressed content as a string,
 *        using an output block_size of 16KB.
 *
 * @param str A view into the source buffer.
 * @return Status or the decompressed content as a string.
 */
inline StatusOr<std::string> StrInflate(std::string_view str) { return StrInflate(str, 16384); }

}  // namespace zlib
}  // namespace pl
