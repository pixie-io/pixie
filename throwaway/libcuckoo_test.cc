#include <gtest/gtest.h>

#include <string>

#include <libcuckoo/cuckoohash_map.hh>

TEST(libcuckoo, basic) {
  cuckoohash_map<int, std::string> table;
  table.insert(1, "abcd");
  table.insert(2, "def");

  const auto& val = table.find(2);
  EXPECT_EQ("def", val);
}
