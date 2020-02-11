#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <sole.hpp>

#include "src/common/base/base.h"
#include "src/common/base/inet_utils.h"

namespace pl {
namespace stirling {
namespace cass {

// See section 3 of
// https://git-wip-us.apache.org/repos/asf?p=cassandra.git;a=blob_plain;f=doc/native_protocol_v3.spec
// for a discussion on types.

// Some complex CQL types defined in the spec.
using StringList = std::vector<std::string>;
using StringMap = std::map<std::string, std::string>;
using StringMultiMap = std::map<std::string, StringList>;

/**
 * TypeDecoder provides a structured interface to process the bytes of a CQL frame body.
 *
 * After creating the decoder, successive calls to the Extract<Type> functions will process
 * the bytes as the desired type.
 *
 * If there are not enough bytes to process the type, an error Status will be returned.
 * The decoder will then be in an undefined state, and the result of any subsequent calls
 * to any Extract functions are also undefined.
 */
class TypeDecoder {
 public:
  /**
   * Create a frame decoder.
   *
   * @param buf A string_view into the body of the CQL frame.
   */
  explicit TypeDecoder(std::string_view buf) : buf_(buf) {}

  // [int] A 4 bytes signed integer.
  StatusOr<int32_t> ExtractInt();

  // [long] A 8 bytes signed integer.
  StatusOr<int64_t> ExtractLong();

  // [short] A 2 bytes unsigned integer.
  StatusOr<uint16_t> ExtractShort();

  // [byte]
  StatusOr<uint8_t> ExtractByte();

  // [string] A [short] n, followed by n bytes representing an UTF-8 string.
  StatusOr<std::string> ExtractString();

  // [long string] An [int] n, followed by n bytes representing an UTF-8 string.
  StatusOr<std::string> ExtractLongString();

  // [uuid] A 16 bytes long uuid.
  StatusOr<sole::uuid> ExtractUUID();

  // [string list] A [short] n, followed by n [string].
  StatusOr<StringList> ExtractStringList();

  // [bytes] A [int] n, followed by n bytes if n >= 0. If n < 0,
  //         no byte should follow and the value represented is `null`.
  StatusOr<std::basic_string<uint8_t>> ExtractBytes();

  // [short bytes]  A [short] n, followed by n bytes if n >= 0.
  StatusOr<std::basic_string<uint8_t>> ExtractShortBytes();

  // [option] A pair of <id><value> where <id> is a [short] representing
  //          the option id and <value> depends on that option (and can be
  //          of size 0). The supported id (and the corresponding <value>)
  //          will be described when this is used.
  // TODO(oazizi): Add an extract function for this type.

  // [option list]  A [short] n, followed by n [option].
  // TODO(oazizi): Add an extract function for this type.

  // [inet] An address (ip and port) to a node. It consists of one
  //        [byte] n, that represents the address size, followed by n
  //        [byte] representing the IP address (in practice n can only be
  //        either 4 (IPv4) or 16 (IPv6)), following by one [int]
  //        representing the port.
  StatusOr<SockAddr> ExtractInet();

  // [consistency]  A consistency level specification. This is a [short]
  //               representing a consistency level ...
  // TODO(oazizi): Add an extract function for this type.

  // [string map] A [short] n, followed by n pair <k><v> where <k> and <v>
  //              are [string].
  StatusOr<StringMap> ExtractStringMap();

  // [string multimap] A [short] n, followed by n pair <k><v> where <k> is a
  //                   [string] and <v> is a [string list].
  StatusOr<StringMultiMap> ExtractStringMultiMap();

  /**
   * Whether processing has reached end-of-frame.
   */
  bool eof() { return buf_.empty(); }

 private:
  template <typename TIntType>
  StatusOr<TIntType> ExtractIntCore();

  template <typename TCharType>
  StatusOr<std::basic_string<TCharType>> ExtractBytesCore(int64_t len);

  template <typename TCharType, size_t N>
  Status ExtractBytesCore(TCharType* out);

  std::string_view buf_;
};

}  // namespace cass
}  // namespace stirling
}  // namespace pl
