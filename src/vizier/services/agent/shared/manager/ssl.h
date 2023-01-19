/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
