#include <fstream>

#include "src/common/system/config.h"
#include "src/common/testing/testing.h"
#include "src/stirling/utils/linux_headers.h"

namespace px {
namespace stirling {
namespace utils {

using ::px::testing::TempDir;

bool operator==(const KernelVersion& a, const KernelVersion& b) {
  return (a.version == b.version) && (a.major_rev == b.major_rev) && (a.minor_rev == b.minor_rev);
}

TEST(LinuxHeadersUtils, LinuxVersionCode) {
  KernelVersion version{4, 18, 1};
  EXPECT_EQ(version.code(), 266753);
}

TEST(LinuxHeadersUtils, ParseKernelVersionString) {
  EXPECT_OK_AND_EQ(ParseKernelVersionString("4.18.0-25-generic"), (KernelVersion{4, 18, 0}));
  EXPECT_OK_AND_EQ(ParseKernelVersionString("4.18.4"), (KernelVersion{4, 18, 4}));
  EXPECT_NOT_OK(ParseKernelVersionString("4.18."));
  EXPECT_NOT_OK(ParseKernelVersionString("linux-4.18.0-25-generic"));
}

TEST(LinuxHeadersUtils, GetKernelVersion) {
  StatusOr<KernelVersion> kernel_version_status = GetKernelVersion();
  ASSERT_OK(kernel_version_status);
  KernelVersion kernel_version = kernel_version_status.ValueOrDie();

  // We don't know on what host this test will run, so we don't know what the version code will be.
  // But we can put some bounds, to check for obvious screw-ups.
  // We assume test will run on a Linux machine with kernel 3.x.x or higher,
  // and version 9.x.x or lower.
  // Yes, we're being very generous here, but we want this test to pass the test of time.
  EXPECT_GE(kernel_version.code(), 0x030000);
  EXPECT_LE(kernel_version.code(), 0x090000);
}

TEST(LinuxHeadersUtils, GetKernelVersionUbuntu) {
  // Setup: Point to a custom /proc filesystem.
  std::string orig_host_path = system::Config::GetInstance().host_path();
  system::FLAGS_host_path = testing::TestFilePath("src/stirling/utils/testdata/dummy_host_ubuntu");
  system::Config::ResetInstance();

  // Main test.
  StatusOr<KernelVersion> kernel_version_status = GetKernelVersion();
  ASSERT_OK(kernel_version_status);
  KernelVersion kernel_version = kernel_version_status.ValueOrDie();
  EXPECT_EQ(kernel_version.code(), 0x050441);

  // Restore config for other tests.
  system::FLAGS_host_path = orig_host_path;
  system::Config::ResetInstance();
}

TEST(LinuxHeadersUtils, GetKernelVersionDebian) {
  // Setup: Point to a custom /proc filesystem.
  std::string orig_host_path = system::Config::GetInstance().host_path();
  system::FLAGS_host_path = testing::TestFilePath("src/stirling/utils/testdata/dummy_host_debian");
  system::Config::ResetInstance();

  // Main test.
  StatusOr<KernelVersion> kernel_version_status = GetKernelVersion();
  ASSERT_OK(kernel_version_status);
  KernelVersion kernel_version = kernel_version_status.ValueOrDie();
  EXPECT_EQ(kernel_version.code(), 0x041398);

  // Restore config for other tests.
  system::FLAGS_host_path = orig_host_path;
  system::Config::ResetInstance();
}

TEST(LinuxHeadersUtils, KernelHeadersDistance) {
  // Distances should always be positive.
  EXPECT_GT(KernelHeadersDistance({4, 14, 255}, {4, 15, 10}), 0);

  // Order shouldn't matter.
  EXPECT_EQ(KernelHeadersDistance({4, 14, 255}, {4, 15, 255}),
            KernelHeadersDistance({4, 15, 255}, {4, 14, 255}));

  EXPECT_GT(KernelHeadersDistance({4, 14, 1}, {4, 15, 5}),
            KernelHeadersDistance({4, 14, 1}, {4, 14, 2}));

  // A change in the major version is considered larger than any change in the minor version.
  EXPECT_GT(KernelHeadersDistance({4, 14, 255}, {4, 15, 0}),
            KernelHeadersDistance({4, 14, 255}, {4, 14, 0}));
}

TEST(LinuxHeadersUtils, ModifyVersion) {
  // Test Setup

  // Use 000000 (which is not a real kernel version), so we can detect changes.
  std::string version_h_original = R"(#define LINUX_VERSION_CODE 000000
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
)";

  TempDir tmp_dir;

  std::filesystem::path version_h_dir = tmp_dir.path() / "include/generated/uapi/linux";
  std::filesystem::create_directories(version_h_dir);
  std::string version_h_filename = version_h_dir / "version.h";

  // Write the original file to disk.
  ASSERT_OK(WriteFileFromString(version_h_filename, version_h_original));

  // Functions Under Test

  StatusOr<KernelVersion> host_linux_version = GetKernelVersion();
  ASSERT_OK(host_linux_version);
  uint32_t host_linux_version_code = host_linux_version.ValueOrDie().code();
  EXPECT_GT(host_linux_version_code, 0);

  EXPECT_OK(ModifyKernelVersion(tmp_dir.path(), host_linux_version_code));

  // Read the file into a string.
  std::string expected_contents_template =
      "#define LINUX_VERSION_CODE $0\n"
      "#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))\n";

  std::string expected_contents =
      absl::Substitute(expected_contents_template, host_linux_version_code);

  EXPECT_OK_AND_EQ(ReadFileToString(version_h_filename), expected_contents);
}

const std::string_view kExpectedConfigContents = R"(#define CONFIG_64BIT 1
#define CONFIG_X86_64 1
#define CONFIG_X86 1
#define CONFIG_INSTRUCTION_DECODER 1
#define CONFIG_OUTPUT_FORMAT "elf64-x86-64"
#define CONFIG_ARCH_DEFCONFIG "arch/x86/configs/x86_64_defconfig"
#define CONFIG_LOCKDEP_SUPPORT 1
#define CONFIG_STACKTRACE_SUPPORT 1
#define CONFIG_MMU 1
#define CONFIG_X86_64_SMP 1
#define CONFIG_ARCH_SUPPORTS_UPROBES 1
#define CONFIG_FIX_EARLYCON_MEM 1
#define CONFIG_PGTABLE_LEVELS 4
#define CONFIG_DEFCONFIG_LIST "/lib/modules/$UNAME_RELEASE/.config"
#define CONFIG_IRQ_WORK 1
#define CONFIG_BUILDTIME_EXTABLE_SORT 1
#define CONFIG_THREAD_INFO_IN_TASK 1
#define CONFIG_HZ 250
#define CONFIG_OPROFILE_MODULE 1
)";

std::string PrepareAutoConf(std::filesystem::path linux_headers_dir) {
  std::filesystem::path include_generated_dir = linux_headers_dir / "include/generated";
  std::filesystem::create_directories(include_generated_dir);
  std::string autoconf_h_filename = include_generated_dir / "autoconf.h";
  return autoconf_h_filename;
}

TEST(LinuxHeadersUtils, GenAutoConf) {
  const std::string kConfigFile = testing::TestFilePath("src/stirling/utils/testdata/config");

  TempDir linux_headers_dir;
  std::string autoconf_h_filename = PrepareAutoConf(linux_headers_dir.path());

  int hz;
  ASSERT_OK(GenAutoConf(linux_headers_dir.path(), kConfigFile, &hz));
  ASSERT_OK_AND_EQ(ReadFileToString(autoconf_h_filename), kExpectedConfigContents);
}

TEST(LinuxHeadersUtils, GenAutoConfGz) {
  const std::string kConfigFile = testing::TestFilePath("src/stirling/utils/testdata/config.gz");

  TempDir linux_headers_dir;
  std::string autoconf_h_filename = PrepareAutoConf(linux_headers_dir.path());

  int hz;
  ASSERT_OK(GenAutoConf(linux_headers_dir.path(), kConfigFile, &hz));
  ASSERT_OK_AND_EQ(ReadFileToString(autoconf_h_filename), kExpectedConfigContents);
}

TEST(LinuxHeadersUtils, FindClosestPackagedLinuxHeaders) {
  const std::string kTestSrcDir =
      testing::TestFilePath("src/stirling/utils/testdata/dummy_header_packages");

  {
    ASSERT_OK_AND_ASSIGN(PackagedLinuxHeadersSpec match,
                         FindClosestPackagedLinuxHeaders(kTestSrcDir, KernelVersion{4, 4, 18}));
    EXPECT_EQ(match.path.string(),
              "src/stirling/utils/testdata/dummy_header_packages/linux-headers-4.14.176.tar.gz");
  }

  {
    ASSERT_OK_AND_ASSIGN(PackagedLinuxHeadersSpec match,
                         FindClosestPackagedLinuxHeaders(kTestSrcDir, KernelVersion{4, 15, 10}));
    EXPECT_EQ(match.path.string(),
              "src/stirling/utils/testdata/dummy_header_packages/linux-headers-4.14.176.tar.gz");
  }

  {
    ASSERT_OK_AND_ASSIGN(PackagedLinuxHeadersSpec match,
                         FindClosestPackagedLinuxHeaders(kTestSrcDir, KernelVersion{4, 18, 1}));
    EXPECT_EQ(match.path.string(),
              "src/stirling/utils/testdata/dummy_header_packages/linux-headers-4.18.20.tar.gz");
  }

  {
    ASSERT_OK_AND_ASSIGN(PackagedLinuxHeadersSpec match,
                         FindClosestPackagedLinuxHeaders(kTestSrcDir, KernelVersion{5, 0, 0}));
    EXPECT_EQ(match.path.string(),
              "src/stirling/utils/testdata/dummy_header_packages/linux-headers-5.3.18.tar.gz");
  }

  {
    ASSERT_OK_AND_ASSIGN(PackagedLinuxHeadersSpec match,
                         FindClosestPackagedLinuxHeaders(kTestSrcDir, KernelVersion{5, 7, 20}));
    EXPECT_EQ(match.path.string(),
              "src/stirling/utils/testdata/dummy_header_packages/linux-headers-5.3.18.tar.gz");
  }
}

}  // namespace utils
}  // namespace stirling
}  // namespace px
