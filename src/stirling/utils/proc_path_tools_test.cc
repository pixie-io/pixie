#include "src/stirling/utils/proc_path_tools.h"

#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/test_container.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {

using ::testing::Contains;
using ::testing::EndsWith;
using ::testing::StartsWith;

TEST(ProcPathToolsContainerTest, ResolveFunctions) {
  DummyTestContainer container;
  ASSERT_OK(container.Run());

  std::filesystem::path proc_pid = absl::Substitute("/proc/$0", container.process_pid());

  ASSERT_OK_AND_ASSIGN(std::filesystem::path root_dir, ResolveProcessRootDir(proc_pid));
  EXPECT_THAT(root_dir, StartsWith("/var/lib/docker/overlay2/"));
  EXPECT_THAT(root_dir, EndsWith("/merged"));

  ASSERT_OK_AND_ASSIGN(std::filesystem::path process_path,
                       ResolveProcessPath(proc_pid, "/app/foo"));
  EXPECT_THAT(process_path, StartsWith("/var/lib/docker/overlay2/"));
  EXPECT_THAT(process_path, EndsWith("/merged/app/foo"));

  ASSERT_OK_AND_ASSIGN(std::filesystem::path proc_exe, ResolveProcExe(proc_pid));
  EXPECT_THAT(proc_exe, StartsWith("/var/lib/docker/overlay2/"));
  EXPECT_THAT(proc_exe, EndsWith("/merged/usr/local/bin/python3.7"));

  ASSERT_OK_AND_ASSIGN(std::filesystem::path pid_binary, ResolvePIDBinary(container.process_pid()));
  EXPECT_THAT(pid_binary, StartsWith("/var/lib/docker/overlay2/"));
  EXPECT_THAT(pid_binary, EndsWith("/merged/usr/local/bin/python3.7"));
}

// Disabled because on Jenkins, proc_path_tools discovers the Jenkins container,
// and this test fails. This test should only be run locally outside a container.
// TODO(oazizi): Investigate a fix.
TEST(ProcPathToolsNonContainerTest, DISABLED_ResolveFunctions) {
  std::filesystem::path proc_pid = "/proc/self";

  EXPECT_OK_AND_THAT(ResolveProcessRootDir(proc_pid), "");

  EXPECT_OK_AND_THAT(ResolveProcessPath(proc_pid, "/app/foo"), "/app/foo");

  EXPECT_OK_AND_THAT(ResolveProcExe(proc_pid), EndsWith("src/stirling/utils/proc_path_tools_test"));

  EXPECT_OK_AND_THAT(ResolvePIDBinary(getpid()),
                     EndsWith("src/stirling/utils/proc_path_tools_test"));
}

TEST(FileSystemResolver, Resolve) {
  DummyTestContainer container;
  ASSERT_OK(container.Run());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FileSystemResolver> fs_resolver,
                       FileSystemResolver::Create(container.process_pid()));

  {
    ASSERT_OK_AND_ASSIGN(std::filesystem::path path, fs_resolver->ResolveMountPoint("/"));
    EXPECT_THAT(path, StartsWith("/var/lib/docker/overlay2/"));
    EXPECT_THAT(path, EndsWith("/merged"));
  }

  {
    ASSERT_OK_AND_ASSIGN(std::filesystem::path path, fs_resolver->ResolvePath("/app/foo"));
    EXPECT_THAT(path, StartsWith("/var/lib/docker/overlay2/"));
    EXPECT_THAT(path, EndsWith("/merged/app/foo"));
  }

  { ASSERT_NOT_OK(fs_resolver->ResolveMountPoint("/bogus")); }
}

}  // namespace stirling
}  // namespace pl
