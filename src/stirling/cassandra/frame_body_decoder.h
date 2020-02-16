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

// See section 4.2.5.2 of
// https://git-wip-us.apache.org/repos/asf?p=cassandra.git;a=blob_plain;f=doc/native_protocol_v3.spec
enum class DataType : uint16_t {
  kCustom = 0x0000,
  kAscii = 0x0001,
  kBigint = 0x0002,
  kBlob = 0x0003,
  kBoolean = 0x0004,
  kCounter = 0x0005,
  kDecimal = 0x0006,
  kDouble = 0x0007,
  kFloat = 0x0008,
  kInt = 0x0009,
  kTimestamp = 0x000B,
  kUuid = 0x000C,
  kVarchar = 0x000D,
  kVarint = 0x000E,
  kTimeuuid = 0x000F,
  kInet = 0x0010,
  kList = 0x0020,
  kMap = 0x0021,
  kSet = 0x0022,
  kUDT = 0x0030,
  kTuple = 0x0031,
};

struct Option {
  DataType type;

  // Value is only used if DataType is kCustom.
  std::string value;

  // TODO(oazizi): Store the additional information if DataType is kList/kMap/kSet/kUDT/kTuple.
};

// TODO(oazizi): Consider using std::optional when values are optional in the structs below.

// QueryParameters is a complex type used in QUERY and EXECUTE requests.
// <query_parameters> is composed of:
// <consistency><flags>[<n>[name_1]<value_1>...[name_n]<value_n>]
// [<result_page_size>][<paging_state>][<serial_consistency>][<timestamp>]
// See section 4.1.4 of the spec for more details.
struct QueryParameters {
  uint16_t consistency;
  uint16_t flags;
  // TODO(oazizi): Consider merging values and names into a struct of its own.
  std::vector<std::basic_string<uint8_t>> values;
  std::vector<std::string> names;
  int32_t page_size = 0;
  std::basic_string<uint8_t> paging_state;
  uint16_t serial_consistency = 0;
  int64_t timestamp = 0;
};

// <col_spec> is composed_of:
// (<ksname><tablename>)?<name><type>
// See section 4.2.5.2 of the spec for more details.
struct ColSpec {
  std::string ks_name;
  std::string table_name;
  std::string name;
  Option type;
};

// <metadata> is composed of:
// <flags><columns_count>[<paging_state>][<global_table_spec>?<col_spec_1>...<col_spec_n>]
// See section 4.2.5.2 of the spec for more details.
struct ResultMetadata {
  int32_t flags;
  int32_t columns_count;
  std::basic_string<uint8_t> paging_state;
  std::string gts_keyspace_name;
  std::string gts_table_name;
  std::vector<ColSpec> col_specs;
};

/**
 * FrameBodyDecoder provides a structured interface to process the bytes of a CQL frame body.
 *
 * After creating the decoder, successive calls to the Extract<Type> functions will process
 * the bytes as the desired type.
 *
 * If there are not enough bytes to process the type, an error Status will be returned.
 * The decoder will then be in an undefined state, and the result of any subsequent calls
 * to any Extract functions are also undefined.
 */
class FrameBodyDecoder {
 public:
  /**
   * Create a frame decoder.
   *
   * @param buf A string_view into the body of the CQL frame.
   */
  explicit FrameBodyDecoder(std::string_view buf) : buf_(buf) {}

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
  StatusOr<Option> ExtractOption();

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

  // Extracts query parameters, which is a complex type. See struct for details.
  StatusOr<QueryParameters> ExtractQueryParameters();

  // Extracts result metadata, which is a complex type. See struct for details.
  StatusOr<ResultMetadata> ExtractResultMetadata();

  /**
   * Whether processing has reached end-of-frame.
   */
  bool eof() { return buf_.empty(); }

  Status ExpectEOF() {
    if (!eof()) {
      return error::Internal("There are still $0 bytes left", buf_.size());
    }
    return Status::OK();
  }

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
