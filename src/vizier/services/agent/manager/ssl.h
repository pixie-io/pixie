#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>

#include "src/common/event/nats.h"

namespace px {
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
  static grpc::SslCredentialsOptions DefaultGRPCClientCredsOpts();

  /**
   * Returns the default GRPC client credentials.
   */
  static std::shared_ptr<grpc::ChannelCredentials> DefaultGRPCClientCreds();

  /**
   * Returns the default GRPC server credentials.
   */
  static std::shared_ptr<grpc::ServerCredentials> DefaultGRPCServerCreds();

  /*
   * Returns the defaul creds for NATS.
   */
  static std::unique_ptr<px::event::NATSTLSConfig> DefaultNATSCreds();
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
