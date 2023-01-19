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

#include "src/vizier/services/agent/shared/manager/ssl.h"
#include "src/common/base/base.h"

DEFINE_bool(disable_SSL, gflags::BoolFromEnv("PL_DISABLE_SSL", false), "Disable GRPC SSL");

DEFINE_string(client_tls_cert,
              gflags::StringFromEnv("PL_CLIENT_TLS_CERT", "../../services/certs/client.crt"),
              "The GRPC client TLS cert");

DEFINE_string(client_tls_key,
              gflags::StringFromEnv("PL_CLIENT_TLS_KEY", "../../services/certs/client.key"),
              "The GRPC client TLS key");

DEFINE_string(tls_ca_crt, gflags::StringFromEnv("PL_TLS_CA_CERT", "../../services/certs/ca.crt"),
              "The GRPC CA cert");

namespace px {
namespace vizier {
namespace agent {

using px::event::NATSTLSConfig;

bool SSL::Enabled() { return !FLAGS_disable_SSL; }

grpc::SslCredentialsOptions SSL::DefaultGRPCClientCredsOpts() {
  grpc::SslCredentialsOptions ssl_opts;
  ssl_opts.pem_root_certs = px::FileContentsOrDie(FLAGS_tls_ca_crt);
  ssl_opts.pem_cert_chain = px::FileContentsOrDie(FLAGS_client_tls_cert);
  ssl_opts.pem_private_key = px::FileContentsOrDie(FLAGS_client_tls_key);
  return ssl_opts;
}

std::shared_ptr<grpc::ChannelCredentials> SSL::DefaultGRPCClientCreds() {
  return SSL::Enabled() ? grpc::SslCredentials(SSL::DefaultGRPCClientCredsOpts())
                        : grpc::InsecureChannelCredentials();
}

std::unique_ptr<NATSTLSConfig> SSL::DefaultNATSCreds() {
  auto tls_config = std::make_unique<NATSTLSConfig>();
  if (!SSL::Enabled()) {
    return tls_config;
  }
  tls_config->ca_cert = FLAGS_tls_ca_crt;
  tls_config->tls_cert = FLAGS_client_tls_cert;
  tls_config->tls_key = FLAGS_client_tls_key;
  return tls_config;
}

std::shared_ptr<grpc::ServerCredentials> SSL::DefaultGRPCServerCreds() {
  if (!SSL::Enabled()) {
    return grpc::InsecureServerCredentials();
  }
  grpc::SslServerCredentialsOptions ssl_opts;
  ssl_opts.pem_root_certs = px::FileContentsOrDie(FLAGS_tls_ca_crt);
  auto pem_key = px::FileContentsOrDie(FLAGS_client_tls_key);
  auto pem_cert = px::FileContentsOrDie(FLAGS_client_tls_cert);
  ssl_opts.pem_key_cert_pairs.push_back({pem_key, pem_cert});
  return grpc::SslServerCredentials(ssl_opts);
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
