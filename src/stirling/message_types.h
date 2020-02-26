#pragma once

#include "src/stirling/cql/types.h"
#include "src/stirling/http/types.h"
#include "src/stirling/http2/http2.h"
#include "src/stirling/http2u/types.h"
#include "src/stirling/mysql/types.h"

namespace pl {
namespace stirling {

/**
 * A map from an EntryType to MessageType.
 * Example usage:
 *   GetMessageType<mysql::Entry>::type --> mysql::Packet
 *
 * @tparam TRecordType The higher-level entry type, which is they map 'key'.
 */

template <class TRecordType>
struct GetMessageType;

template <>
struct GetMessageType<http::Record> {
  typedef http::Message type;
};

template <>
struct GetMessageType<http2::Record> {
  typedef http2::Frame type;
};

template <>
struct GetMessageType<http2u::Record> {
  typedef http2u::Stream type;
};

template <>
struct GetMessageType<mysql::Record> {
  typedef mysql::Packet type;
};

template <>
struct GetMessageType<cass::Record> {
  typedef cass::Frame type;
};

}  // namespace stirling
}  // namespace pl
