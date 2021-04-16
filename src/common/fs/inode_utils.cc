#include "src/common/fs/inode_utils.h"

namespace px {
namespace fs {

// Extract the inode number from a string that looks like the following: "socket:[32431]"
StatusOr<uint32_t> ExtractInodeNum(std::string_view inode_type_prefix, std::string_view link_str) {
  if (!absl::StartsWith(link_str, inode_type_prefix)) {
    return error::Internal("FD does not appear to be a valid socket inode string. FD link = $0",
                           link_str);
  }

  link_str.remove_prefix(inode_type_prefix.size());
  if (link_str.empty() || link_str.front() != '[' || link_str.back() != ']') {
    return error::Internal("Malformed inode string.");
  }
  link_str.remove_prefix(1);
  link_str.remove_suffix(1);

  uint32_t inode_num;
  if (!absl::SimpleAtoi(link_str, &inode_num)) {
    return error::Internal("Could not parse inode string.");
  }

  return inode_num;
}

}  // namespace fs
}  // namespace px
