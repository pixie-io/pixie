#include "src/stirling/mysql/parse_utils.h"

#include <string>

#include "src/common/base/byte_utils.h"
#include "src/stirling/mysql/mysql.h"

namespace pl {
namespace stirling {
namespace mysql {

StatusOr<int64_t> ProcessLengthEncodedInt(std::string_view s, size_t* offset) {
  // If it is < 0xfb, treat it as a 1-byte integer.
  // If it is 0xfc, it is followed by a 2-byte integer.
  // If it is 0xfd, it is followed by a 3-byte integer.
  // If it is 0xfe, it is followed by a 8-byte integer.

  constexpr uint8_t kLencIntPrefix2b = 0xfc;
  constexpr uint8_t kLencIntPrefix3b = 0xfd;
  constexpr uint8_t kLencIntPrefix8b = 0xfe;

  if (*offset >= s.size()) {
    return error::Internal("Not enough bytes to extract length-encoded int");
  }

  int64_t result;
  int len;
  switch (static_cast<uint8_t>(s[*offset])) {
    case kLencIntPrefix2b:
      len = 2;
      ++*offset;
      break;
    case kLencIntPrefix3b:
      len = 3;
      ++*offset;
      break;
    case kLencIntPrefix8b:
      len = 8;
      ++*offset;
      break;
    default:
      len = 1;
      break;
  }

  if (*offset + len > s.size()) {
    return error::Internal("Not enough bytes to extract length-encoded int");
  }

  result = utils::LittleEndianByteStrToInt<uint64_t>(s.substr(*offset, len));
  *offset += len;

  return result;
}

Status DissectStringParam(std::string_view msg, size_t* param_offset, ParamPacket* packet) {
  PL_ASSIGN_OR_RETURN(int param_length, ProcessLengthEncodedInt(msg, param_offset));
  if (msg.size() < *param_offset + param_length) {
    return error::Internal("Not enough bytes to dissect string param.");
  }
  packet->value = msg.substr(*param_offset, param_length);
  *param_offset += param_length;
  return Status::OK();
}

template <size_t length>
Status DissectIntParam(std::string_view msg, size_t* offset, ParamPacket* packet) {
  if (msg.size() < *offset + length) {
    return error::Internal("Not enough bytes to dissect int param.");
  }
  packet->value =
      std::to_string(utils::LittleEndianByteStrToInt<int64_t>(msg.substr(*offset, length)));
  *offset += length;
  return Status::OK();
}

// Template instantiations to include in the object file.
template Status DissectIntParam<1>(std::string_view msg, size_t* offset, ParamPacket* packet);
template Status DissectIntParam<2>(std::string_view msg, size_t* offset, ParamPacket* packet);
template Status DissectIntParam<4>(std::string_view msg, size_t* offset, ParamPacket* packet);
template Status DissectIntParam<8>(std::string_view msg, size_t* offset, ParamPacket* packet);

template <typename TFloatType>
Status DissectFloatParam(std::string_view msg, size_t* offset, ParamPacket* packet) {
  size_t length = sizeof(TFloatType);
  if (msg.size() < *offset + length) {
    return error::Internal("Not enough bytes to dissect float param.");
  }
  packet->value =
      std::to_string(utils::LittleEndianByteStrToFloat<TFloatType>(msg.substr(*offset, length)));
  *offset += length;
  return Status::OK();
}

// Template instantiations to include in the object file.
template Status DissectFloatParam<float>(std::string_view msg, size_t* offset, ParamPacket* packet);
template Status DissectFloatParam<double>(std::string_view msg, size_t* offset,
                                          ParamPacket* packet);

Status DissectDateTimeParam(std::string_view msg, size_t* offset, ParamPacket* packet) {
  if (msg.size() < *offset + 1) {
    return error::Internal("Not enough bytes to dissect date/time param.");
  }

  uint8_t length = static_cast<uint8_t>(msg[*offset]);
  ++*offset;

  if (msg.size() < *offset + length) {
    return error::Internal("Not enough bytes to dissect date/time param.");
  }
  packet->value = "MySQL DateTime rendering not implemented yet";
  *offset += length;
  return Status::OK();
}

// Spec on how to dissect params is here:
// https://dev.mysql.com/doc/internals/en/binary-protocol-value.html
//
// List of parameter types is followed by list of parameter values,
// so we have two offset pointers, one that points to current type position,
// and one that points to current value position
Status DissectParam(std::string_view msg, size_t* type_offset, size_t* val_offset,
                    ParamPacket* param) {
  param->type = static_cast<MySQLColType>(msg[*type_offset]);
  type_offset += 2;

  switch (param->type) {
    case MySQLColType::kString:
    case MySQLColType::kVarChar:
    case MySQLColType::kVarString:
    case MySQLColType::kEnum:
    case MySQLColType::kSet:
    case MySQLColType::kLongBlob:
    case MySQLColType::kMediumBlob:
    case MySQLColType::kBlob:
    case MySQLColType::kTinyBlob:
    case MySQLColType::kGeometry:
    case MySQLColType::kBit:
    case MySQLColType::kDecimal:
    case MySQLColType::kNewDecimal:
      PL_RETURN_IF_ERROR(DissectStringParam(msg, val_offset, param));
      break;
    case MySQLColType::kTiny:
      PL_RETURN_IF_ERROR(DissectIntParam<1>(msg, val_offset, param));
      break;
    case MySQLColType::kShort:
    case MySQLColType::kYear:
      PL_RETURN_IF_ERROR(DissectIntParam<2>(msg, val_offset, param));
      break;
    case MySQLColType::kLong:
    case MySQLColType::kInt24:
      PL_RETURN_IF_ERROR(DissectIntParam<4>(msg, val_offset, param));
      break;
    case MySQLColType::kLongLong:
      PL_RETURN_IF_ERROR(DissectIntParam<8>(msg, val_offset, param));
      break;
    case MySQLColType::kFloat:
      PL_RETURN_IF_ERROR(DissectFloatParam<float>(msg, val_offset, param));
      break;
    case MySQLColType::kDouble:
      PL_RETURN_IF_ERROR(DissectFloatParam<double>(msg, val_offset, param));
      break;
    case MySQLColType::kDate:
    case MySQLColType::kDateTime:
    case MySQLColType::kTimestamp:
      PL_RETURN_IF_ERROR(DissectDateTimeParam(msg, val_offset, param));
      break;
    case MySQLColType::kTime:
      PL_RETURN_IF_ERROR(DissectDateTimeParam(msg, val_offset, param));
      break;
    case MySQLColType::kNull:
      break;
    default:
      LOG(DFATAL) << absl::Substitute("Unexpected/unhandled column type $0", msg[*type_offset]);
  }

  return Status::OK();
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
