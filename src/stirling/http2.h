#pragma once

#include <nghttp2/nghttp2.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "src/common/base/mixins.h"
#include "src/common/base/status.h"

namespace pl {
namespace stirling {
namespace http2 {

/**
 * @brief Returns a string for a particular type.
 */
std::string_view FrameTypeName(uint8_t type);

/**
 * @brief A wrapper around  nghttp2_frame. nghttp2_frame misses some fields, for example, it has no
 * data body field in nghttp2_data. The payload is a name meant to be generic enough so that it can
 * be used to store such fields for different message types.
 */
class Frame : public NotCopyMoveable {
 public:
  Frame();
  ~Frame();

  Frame(Frame&&) = delete;

  nghttp2_frame frame;
  std::string payload;
};

Status UnpackFrames(std::string_view* buf, std::vector<std::unique_ptr<Frame>>* frames);

// TODO(yzhao): Embed this in Frame or some other data structure.
// TODO(yzhao): Was thinking if this should be put into grpc.h, decide when more code are ready.
struct GRPCMessage {
  // compressed_flag can be 0/1, and is encoded as 1 octet.
  uint8_t compressed_flag;
  uint32_t length;
  std::string message;
};
Status UnpackGRPCMessage(std::string_view* buf, GRPCMessage* payload);

}  // namespace http2
}  // namespace stirling
}  // namespace pl
