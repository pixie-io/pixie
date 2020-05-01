#include <filesystem>
#include <string>

#include "src/common/base/base.h"
#include "src/common/testing/testing.h"

#include "src/common/exec/exec.h"
#include "src/common/minitar/minitar.h"

using ::pl::testing::TestFilePath;

TEST(Minitar, Extract) {
  // Tarball and a reference extracted version for comparison.
  std::filesystem::path tarball = TestFilePath("src/common/minitar/testdata/a.tar.gz");
  std::filesystem::path ref = TestFilePath("src/common/minitar/testdata/ref");

  ::pl::tools::Minitar minitar(tarball);
  EXPECT_OK(minitar.Extract());

  std::string diff_cmd = absl::Substitute("diff a $0", ref.string());
  EXPECT_OK_AND_EQ(::pl::Exec(diff_cmd), "");
}
