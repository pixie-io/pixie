#pragma once
#include <string>

#include "src/common/base/base.h"
#include "src/stirling/mysql/types.h"

namespace pl {
namespace stirling {
namespace mysql {

/**
 * Converts a length encoded int from string to int.
 * https://dev.mysql.com/doc/internals/en/integer.html#packet-Protocol::LengthEncodedInteger
 *
 * @param s the source bytes from which to extract the length-encoded int
 * @param offset the position at which to parse the length-encoded int.
 * The offset will be updated to point to the end position on a successful parse.
 * On an unsuccessful parse, the offset will be in an undefined state.
 *
 * @return int value if parsed, or error if there are not enough bytes to parse the int.
 */
StatusOr<int64_t> ProcessLengthEncodedInt(std::string_view s, size_t* offset);

/**
 * These dissectors are helper functions that parse out a parameter from a packet's raw contents.
 * The offset identifies where to begin the parsing, and the offset is updated to reflect where
 * the parsing ends. The parsed result in placed in the StmtExecuteParam.
 */
// TODO(oazizi): Convert Dissectors to use std::string_view*, and use remove_prefix().
// Then param_offset is no longer needed.

Status DissectStringParam(std::string_view msg, size_t* param_offset, std::string* packet);

template <size_t length>
Status DissectIntParam(std::string_view msg, size_t* param_offset, std::string* packet);

template <typename TFloatType>
Status DissectFloatParam(std::string_view msg, size_t* offset, std::string* packet);

Status DissectDateTimeParam(std::string_view msg, size_t* offset, std::string* packet);

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
