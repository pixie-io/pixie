#pragma once

#include <string>

#include "src/common/base/base.h"
#include "src/common/testing/test_utils/container_runner.h"

namespace pl {

class DummyTestContainer : public ContainerRunner {
 public:
  DummyTestContainer() : ContainerRunner(kImage, kInstanceNamePrefix, kReadyMessage) {}

 private:
  inline static std::string kImage = "gcr.io/google-samples/microservices-demo/emailservice:v0.1.3";
  inline static std::string kInstanceNamePrefix = "dummy_container";
  inline static std::string kReadyMessage = "listening on port: 8080";
};

}  // namespace pl
