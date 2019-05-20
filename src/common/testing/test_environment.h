#pragma once

#include <string>

namespace pl {

class TestEnvironment {
 public:
  static std::string TestRunDir();
  static std::string PathToTestDataFile(const std::string_view &fname);
};

}  // namespace pl
