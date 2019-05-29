#include <zlib.h>
#include <string>

#include "src/common/base/base.h"
#include "src/common/zlib/zlib_wrapper.h"

namespace pl {
namespace zlib {

int Inflate(const char* src, int src_len, char* dst, int dst_len) {
  z_stream strm = {};

  // Setup input buffer.
  strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(src));
  strm.avail_in = src_len;

  // Setup output buffer.
  strm.next_out = reinterpret_cast<Bytef*>(dst);
  strm.avail_out = dst_len;

  int err = -1;
  err = inflateInit2(&strm, MAX_WBITS + 16);
  if (err == Z_OK) {
    err = inflate(&strm, Z_FINISH);
  }
  inflateEnd(&strm);
  return err;
}

StatusOr<std::string> StrInflate(std::string_view str, size_t output_block_size) {
  z_stream zs = {};

  if (inflateInit2(&zs, MAX_WBITS + 16) != Z_OK) {
    return error::Internal("inflateInit2 failed while decompressing.");
  }

  // Setup input buffer.
  zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(str.data()));
  zs.avail_in = str.size();

  int ret;
  std::string outstring;

  // Get the decompressed bytes blockwise using repeated calls to inflate.
  do {
    outstring.resize(outstring.size() + output_block_size);
    zs.next_out = reinterpret_cast<Bytef*>(outstring.data() + zs.total_out);
    zs.avail_out = outstring.size() - zs.total_out;

    ret = inflate(&zs, 0);

    outstring.resize(zs.total_out);
  } while (ret == Z_OK);

  outstring.shrink_to_fit();

  inflateEnd(&zs);

  if (ret != Z_STREAM_END) {
    // An error occurred that was not EOF.
    return error::Internal("Exception during zlib decompression: $0", zs.msg);
  }

  return outstring;
}

}  // namespace zlib
}  // namespace pl
