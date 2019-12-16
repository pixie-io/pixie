#include "src/stirling/obj_tools/obj_tools.h"

#include "src/common/exec/exec.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"

DEFINE_string(go_greeter_client_path, "", "The path to the go greeter client executable.");

namespace pl {
namespace stirling {
namespace obj_tools {

using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::EndsWith;
using ::testing::IsEmpty;
using ::testing::Pair;

// This test does not pass in Jenkins. The failure is that the resolved overlay path was reported as
// non-existent by filesystem::exists(); therefore, GetActiveBinariesTest() ignores the file.
TEST(GetActiveBinariesTest, DISABLED_CaptureTestBinary) {
  const std::map<std::string, std::vector<int>> binaries = GetActiveBinaries("/proc");
  EXPECT_THAT(binaries, Contains(Pair(EndsWith("src/stirling/obj_tools/obj_tools_test"), _)))
      << "Should see the test process itself";
}

TEST(GetSymAddrsTest, SymbolAddress) {
  CHECK(!FLAGS_go_greeter_client_path.empty())
      << "--go_greeter_client_path cannot be empty. You should run this test with bazel.";
  std::map<std::string, std::vector<int>> binaries;
  binaries[FLAGS_go_greeter_client_path] = {1, 2, 3};
  EXPECT_THAT(GetSymAddrs(binaries), ElementsAre(Pair(1, _), Pair(2, _), Pair(3, _)));
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
