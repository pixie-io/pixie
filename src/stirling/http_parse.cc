#include "src/stirling/http_parse.h"

#include <picohttpparser.h>

namespace pl {
namespace stirling {

void ParseMessageBodyChunked(HTTPTraceRecord* record) {
  if (record->http_resp_body.empty()) {
    return;
  }
  phr_chunked_decoder decoder = {};
  char* buf = const_cast<char*>(record->http_resp_body.data());
  size_t buf_size = record->http_resp_body.size();
  ssize_t retval = phr_decode_chunked(&decoder, buf, &buf_size);
  if (retval != -1) {
    // As long as the parse succeeded (-1 indicate parse failure), buf_size is the decoded data
    // size (even if it's incomplete).
    record->http_resp_body.resize(buf_size);
  }
  if (retval >= 0) {
    record->chunking_status = ChunkingStatus::COMPLETE;
  } else {
    record->chunking_status = ChunkingStatus::CHUNKED;
  }
}

}  // namespace stirling
}  // namespace pl
