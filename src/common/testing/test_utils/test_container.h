#pragma once

#include <string>

#include "src/common/base/base.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/container_runner.h"

namespace pl {

// TODO(oazizi): Remove all uses to this container in favor of BusyBoxContainer below.
class DummyTestContainer : public ContainerRunner {
 public:
  DummyTestContainer() : ContainerRunner(kImage, kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kImage =
      "gcr.io/google-samples/microservices-demo/emailservice:v0.1.3";
  static constexpr std::string_view kInstanceNamePrefix = "dummy_container";
  static constexpr std::string_view kReadyMessage = "listening on port: 8080";
};

class BusyBoxContainer : public ContainerRunner {
 public:
  BusyBoxContainer()
      : ContainerRunner(pl::testing::BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/common/testing/test_utils/busybox_container_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "busybox_container";
  static constexpr std::string_view kReadyMessage = "started";
};

}  // namespace pl
