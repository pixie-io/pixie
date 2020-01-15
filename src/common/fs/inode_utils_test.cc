#include <gtest/gtest.h>

#include "src/common/fs/inode_utils.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace fs {

TEST(InodeUtils, ExtractInodeNum) {
  EXPECT_EQ(ExtractInodeNum(kSocketInodePrefix, "socket:[32431]").ValueOrDie(), 32431);
  EXPECT_EQ(ExtractInodeNum(kNetInodePrefix, "net:[1234]").ValueOrDie(), 1234);

  EXPECT_NOT_OK(ExtractInodeNum(kSocketInodePrefix, "socket:[32x31]"));
  EXPECT_NOT_OK(ExtractInodeNum(kSocketInodePrefix, "socket[32431]"));
  EXPECT_NOT_OK(ExtractInodeNum(kNetInodePrefix, "[32431]"));
  EXPECT_NOT_OK(ExtractInodeNum(kNetInodePrefix, "socket:[32431]"));
}

}  // namespace fs
}  // namespace pl
