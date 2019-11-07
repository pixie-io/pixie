#pragma once
#include <string>

#include "src/common/base/base.h"
#include "src/stirling/mysql/mysql.h"

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

Status DissectStringParam(std::string_view msg, size_t* param_offset, ParamPacket* packet);

template <size_t length>
Status DissectIntParam(std::string_view msg, size_t* param_offset, ParamPacket* packet);

template <typename TFloatType>
Status DissectFloatParam(std::string_view msg, size_t* offset, ParamPacket* packet);

Status DissectDateTimeParam(std::string_view msg, size_t* offset, ParamPacket* packet);

Status DissectParam(std::string_view msg, size_t* type_offset, size_t* val_offset,
                    ParamPacket* param);

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
