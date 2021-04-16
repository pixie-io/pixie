#include <zlib.h>
#include <string>

#include "src/common/base/base.h"
#include "src/common/zlib/zlib_wrapper.h"

namespace px {
namespace zlib {

StatusOr<std::string> Inflate(std::string_view in, size_t output_block_size) {
  z_stream zs = {};

  if (inflateInit2(&zs, MAX_WBITS + 16) != Z_OK) {
    return error::Internal("inflateInit2 failed while decompressing.");
  }

  // Setup input buffer.
  zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
  zs.avail_in = in.size();

  int ret;
  std::string out;

  // Get the decompressed bytes blockwise using repeated calls to inflate.
  do {
    out.resize(out.size() + output_block_size);
    zs.next_out = reinterpret_cast<Bytef*>(out.data() + zs.total_out);
    zs.avail_out = out.size() - zs.total_out;

    ret = inflate(&zs, 0);
  } while (ret == Z_OK);

  out.resize(zs.total_out);

  inflateEnd(&zs);

  if (ret != Z_STREAM_END) {
    // An error occurred that was not EOF.
    return error::Internal("Exception during zlib decompression: $0", zs.msg);
  }

  return out;
}

}  // namespace zlib
}  // namespace px
