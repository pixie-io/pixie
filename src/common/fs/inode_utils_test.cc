#include <gtest/gtest.h>

#include "src/common/fs/inode_utils.h"
#include "src/common/testing/testing.h"

namespace px {
namespace fs {

TEST(InodeUtils, ExtractInodeNum) {
  EXPECT_OK_AND_EQ(ExtractInodeNum(kSocketInodePrefix, "socket:[32431]"), 32431);
  EXPECT_OK_AND_EQ(ExtractInodeNum(kNetInodePrefix, "net:[1234]"), 1234);

  // Inodes are 32-bit unsigned numbers, so check for that too.
  EXPECT_OK_AND_EQ(ExtractInodeNum(kNetInodePrefix, "net:[3221225472]"), 3221225472);

  EXPECT_NOT_OK(ExtractInodeNum(kSocketInodePrefix, "socket:[32x31]"));
  EXPECT_NOT_OK(ExtractInodeNum(kSocketInodePrefix, "socket[32431]"));
  EXPECT_NOT_OK(ExtractInodeNum(kNetInodePrefix, "[32431]"));
  EXPECT_NOT_OK(ExtractInodeNum(kNetInodePrefix, "socket:[32431]"));
}

}  // namespace fs
}  // namespace px
