#pragma once

#include <google/protobuf/util/delimited_message_util.h>

#include <iostream>
#include <memory>

namespace pl {
namespace rio {

/**
 * @brief Writes a protobuf message to an std::ostream object. Does not write any type information,
 * so the caller has to manage the mixed types of protobuf messages.
 */
inline bool SerializeToStream(const ::google::protobuf::MessageLite& message,
                              std::ostream* output) {
  return ::google::protobuf::util::SerializeDelimitedToOstream(message, output);
}

/**
 * @brief Reads protobuf messages from an std::istream.
 */
class Reader {
 public:
  Reader(std::istream* input) : zstream_(input) {}

  template <typename ProtobufType>
  bool Read(ProtobufType* msg) {
    return ::google::protobuf::util::ParseDelimitedFromZeroCopyStream(msg, &zstream_, &eof_);
  }

  // This is a bit non-intuitive. ParseDelimitedFromZeroCopyStream() sets eof_; but always will need
  // one additional call. I.e., given an input stream has only one message, after first read, the
  // data is depleted, but eof_ will still be false. The caller needs to continue call Read().
  //
  // The pattern would be:
  // Reader reader(&is);
  // while (!reader.eof()) {
  //   ProtobufType pb;
  //   if (reader.Read(&pb)) {
  //     // Do something.
  //   }
  // }
  bool eof() const { return eof_; }

 private:
  bool eof_ = false;
  ::google::protobuf::io::IstreamInputStream zstream_;
};

}  // namespace rio
}  // namespace pl
