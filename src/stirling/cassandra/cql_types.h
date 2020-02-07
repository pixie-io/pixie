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
 * Extract functions: The following functions take an string_view pointer to raw bytes as
 * input, and process those bytes into the desired CQL type.
 *
 * After processing the string_view pointer will have been updated to point to point to the
 * first unprocessed byte (all processed bytes are removed from the view).
 *
 * If there are not enough bytes to process the type, and error Status will be returned.
 * The buf pointer may have been mutated, so it is up to the caller to have saved the old
 * value if some sort of recovery is desired.
 */

// [int] A 4 bytes signed integer.
StatusOr<int32_t> ExtractInt(std::string_view* buf);

// [long] A 8 bytes signed integer.
StatusOr<int64_t> ExtractLong(std::string_view* buf);

// [short] A 2 bytes unsigned integer.
StatusOr<uint16_t> ExtractShort(std::string_view* buf);

// [byte]
StatusOr<uint8_t> ExtractByte(std::string_view* buf);

// [string] A [short] n, followed by n bytes representing an UTF-8 string.
StatusOr<std::string> ExtractString(std::string_view* buf);

// [long string] An [int] n, followed by n bytes representing an UTF-8 string.
StatusOr<std::string> ExtractLongString(std::string_view* buf);

// [uuid] A 16 bytes long uuid.
StatusOr<sole::uuid> ExtractUUID(std::string_view* buf);

// [string list] A [short] n, followed by n [string].
StatusOr<StringList> ExtractStringList(std::string_view* buf);

// [bytes] A [int] n, followed by n bytes if n >= 0. If n < 0,
//         no byte should follow and the value represented is `null`.
StatusOr<std::basic_string<uint8_t>> ExtractBytes(std::string_view* buf);

// [short bytes]  A [short] n, followed by n bytes if n >= 0.
StatusOr<std::basic_string<uint8_t>> ExtractShortBytes(std::string_view* buf);

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
StatusOr<SockAddr> ExtractInet(std::string_view* buf);

// [consistency]  A consistency level specification. This is a [short]
//               representing a consistency level ...
// TODO(oazizi): Add an extract function for this type.

// [string map] A [short] n, followed by n pair <k><v> where <k> and <v>
//              are [string].
StatusOr<StringMap> ExtractStringMap(std::string_view* buf);

// [string multimap] A [short] n, followed by n pair <k><v> where <k> is a
//                   [string] and <v> is a [string list].
StatusOr<StringMultiMap> ExtractStringMultiMap(std::string_view* buf);

}  // namespace cass
}  // namespace stirling
}  // namespace pl
