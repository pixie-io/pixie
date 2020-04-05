#pragma once

#include <string>

#include "src/common/base/base.h"
#include "src/common/testing/test_utils/container_runner.h"

namespace pl {

class DummyTestContainer : public ContainerRunner {
 public:
  DummyTestContainer() : ContainerRunner(kImage, kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kImage =
      "gcr.io/google-samples/microservices-demo/emailservice:v0.1.3";
  static constexpr std::string_view kInstanceNamePrefix = "dummy_container";
  static constexpr std::string_view kReadyMessage = "listening on port: 8080";
};

}  // namespace pl
