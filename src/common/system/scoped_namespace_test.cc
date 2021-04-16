#include <filesystem>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/base.h"
#include "src/common/system/scoped_namespace.h"
#include "src/common/testing/test_utils/test_container.h"
#include "src/common/testing/testing.h"

using ::testing::Contains;
using ::testing::Not;

namespace px {
namespace system {

class DummyContainer : public ContainerRunner {
 public:
  DummyContainer()
      : ContainerRunner(px::testing::BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/common/system/testdata/dummy_container_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "dummy_container";
  static constexpr std::string_view kReadyMessage = "started";
};

std::vector<std::filesystem::path> ListDir(const std::filesystem::path& dir) {
  using DirectoryIterator = std::filesystem::directory_iterator;
  return std::vector<std::filesystem::path>(DirectoryIterator(dir), DirectoryIterator());
}

// Use mount namespace as a demonstration of ScopedNamespace functionality.
TEST(ScopedNamespaceTest, MountNamespace) {
  DummyContainer container;
  ASSERT_OK(container.Run());

  // This is a path that we can reasonably assume only exists in the DummyContainer.
  const std::filesystem::path kDummyContainerDir("/dummy_dir");

  EXPECT_THAT(ListDir("/"), Not(Contains(kDummyContainerDir)));

  // Create a scope under which we will switch namespaces.
  // The namespace will apply to all code in the scope, but once it exits,
  // the original namespace is restored (RAII style).
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<ScopedNamespace> scoped_namespace,
                         ScopedNamespace::Create(container.process_pid(), "mnt"));

    // Now that we're in the container's mount namespace, we expect to see its filesystem.
    EXPECT_THAT(ListDir("/"), Contains(kDummyContainerDir));
  }

  EXPECT_THAT(ListDir("/"), Not(Contains(kDummyContainerDir)));
}

}  // namespace system
}  // namespace px
