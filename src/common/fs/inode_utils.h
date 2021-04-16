#pragma once

#include "src/common/base/base.h"

namespace px {
namespace fs {

constexpr std::string_view kSocketInodePrefix = ConstStringView("socket:");
constexpr std::string_view kNetInodePrefix = ConstStringView("net:");

/**
 * Extract the inode number from a string that looks like the following: "socket:[32431]"
 * that come from a readlink of a file that links to an inode.
 *
 * @param inode_type_prefix The type of inode to parse for (e.g. "socket:").
 * @param link_str The link string to the inode (e.g. "socket:[32431]").
 */
StatusOr<uint32_t> ExtractInodeNum(std::string_view inode_type_prefix, std::string_view link_str);

}  // namespace fs
}  // namespace px
