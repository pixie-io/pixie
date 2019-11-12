#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>

#include "src/vizier/services/agent/controller/controller.h"

namespace pl {
namespace vizier {
namespace agent {

/**
 * SSL has information about SSL as used by the agents (Pem/Kelvin).
 */
class SSL {
 public:
  /**
   * True if SSL is enabled.
   */

  static bool Enabled();
  /**
   * Returns the default SSL Client options. Only valid when SSL is enabled.
   */
  static grpc::SslCredentialsOptions DefaultGRPCClientCreds();

  /*
   * Returns the defaul creds for NATS. Only valid when SSL is enabled.
   */
  static std::unique_ptr<Controller::VizierNATSTLSConfig> DefaultNATSCreds();
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
