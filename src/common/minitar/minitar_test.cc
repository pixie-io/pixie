#include <filesystem>
#include <string>

#include "src/common/base/base.h"
#include "src/common/testing/testing.h"

#include "src/common/exec/exec.h"
#include "src/common/minitar/minitar.h"

using ::px::testing::TestFilePath;

TEST(Minitar, Extract) {
  // Tarball and a reference extracted version for comparison.
  std::filesystem::path tarball = TestFilePath("src/common/minitar/testdata/a.tar.gz");
  std::filesystem::path ref = TestFilePath("src/common/minitar/testdata/ref");

  ::px::tools::Minitar minitar(tarball);
  EXPECT_OK(minitar.Extract());

  std::string diff_cmd = absl::Substitute("diff -rs a $0", ref.string());
  const std::string kExpectedDiffResult =
      "Files a/bar and src/common/minitar/testdata/ref/bar are identical\n"
      "Files a/foo and src/common/minitar/testdata/ref/foo are identical\n";
  EXPECT_OK_AND_EQ(::px::Exec(diff_cmd), kExpectedDiffResult);
}

TEST(Minitar, ExtractTo) {
  // Tarball and a reference extracted version for comparison.
  std::filesystem::path tarball = TestFilePath("src/common/minitar/testdata/a.tar.gz");
  std::filesystem::path ref = TestFilePath("src/common/minitar/testdata/ref");

  ::px::tools::Minitar minitar(tarball);
  EXPECT_OK(minitar.Extract("src/common/minitar/testdata"));

  std::string diff_cmd =
      absl::Substitute("diff -rs src/common/minitar/testdata/a $0", ref.string());
  const std::string kExpectedDiffResult =
      "Files src/common/minitar/testdata/a/bar and src/common/minitar/testdata/ref/bar are "
      "identical\n"
      "Files src/common/minitar/testdata/a/foo and src/common/minitar/testdata/ref/foo are "
      "identical\n";
  EXPECT_OK_AND_EQ(::px::Exec(diff_cmd), kExpectedDiffResult);
}
