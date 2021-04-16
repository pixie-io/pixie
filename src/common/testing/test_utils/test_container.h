#pragma once

#include <string>

#include "src/common/base/base.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/container_runner.h"

namespace px {

// TODO(oazizi): Remove all uses to this container in favor of SleepContainer below.
class DummyTestContainer : public ContainerRunner {
 public:
  DummyTestContainer() : ContainerRunner(kImage, kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kImage =
      "gcr.io/google-samples/microservices-demo/emailservice:v0.1.3";
  static constexpr std::string_view kInstanceNamePrefix = "dummy_container";
  static constexpr std::string_view kReadyMessage = "listening on port: 8080";
};

class SleepContainer : public ContainerRunner {
 public:
  SleepContainer()
      : ContainerRunner(px::testing::BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/common/testing/test_utils/sleep_container_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "sleep_container";
  static constexpr std::string_view kReadyMessage = "started";
};

}  // namespace px
