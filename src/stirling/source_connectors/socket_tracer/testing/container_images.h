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

#include <string>

#include "src/common/exec/exec.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/container_runner.h"

namespace px {
namespace stirling {
namespace testing {

//-----------------------------------------------------------------------------
// General
//-----------------------------------------------------------------------------

class RubyContainer : public ContainerRunner {
 public:
  RubyContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/ruby_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "ruby";
  static constexpr std::string_view kReadyMessage = "";
};

//-----------------------------------------------------------------------------
// HTTP/HTTPS
//-----------------------------------------------------------------------------

namespace internal {

// A helper function useful for the Nginx images below.
int32_t GetNginxWorkerPID(int32_t pid) {
  // Nginx has a master process and a worker process. We need the PID of the worker process.
  int worker_pid;
  std::string pid_str = px::Exec(absl::Substitute("pgrep -P $0", pid)).ValueOrDie();
  CHECK(absl::SimpleAtoi(pid_str, &worker_pid));
  LOG(INFO) << absl::Substitute("Worker thread PID: $0", worker_pid);
  return worker_pid;
}

}  // namespace internal

class NginxOpenSSL_1_1_0_Container : public ContainerRunner {
 public:
  NginxOpenSSL_1_1_0_Container()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

  int32_t NginxWorkerPID() const { return internal::GetNginxWorkerPID(process_pid()); }

 private:
  // Image is a modified nginx image created through bazel rules, and stored as a tar file.
  // It is not pushed to any repo.
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/"
      "nginx_openssl_1_1_0_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "nginx";
  static constexpr std::string_view kReadyMessage = "";
};

class NginxOpenSSL_1_1_1_Container : public ContainerRunner {
 public:
  NginxOpenSSL_1_1_1_Container()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

  int32_t NginxWorkerPID() const { return internal::GetNginxWorkerPID(process_pid()); }

 private:
  // Image is a modified nginx image created through bazel rules, and stored as a tar file.
  // It is not pushed to any repo.
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/"
      "nginx_openssl_1_1_1_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "nginx";
  static constexpr std::string_view kReadyMessage = "";
};

class CurlContainer : public ContainerRunner {
 public:
  CurlContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/curl_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "curl";
  static constexpr std::string_view kReadyMessage = "";
};

class Node12_3_1Container : public ContainerRunner {
 public:
  Node12_3_1Container()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/node_12_3_1_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "node_server";
  static constexpr std::string_view kReadyMessage = "Nodejs https server started!";
};

class Node14_18_1AlpineContainer : public ContainerRunner {
 public:
  Node14_18_1AlpineContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/"
      "node_14_18_1_alpine_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "node_server";
  static constexpr std::string_view kReadyMessage = "Nodejs https server started!";
};

class NodeClientContainer : public ContainerRunner {
 public:
  NodeClientContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/"
      "node_14_18_1_alpine_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "node_client";
  static constexpr std::string_view kReadyMessage = "";
};

class Go1_16_TLSServerContainer : public ContainerRunner {
 public:
  Go1_16_TLSServerContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_https/server/golang_1_16_https_server.tar";
  static constexpr std::string_view kContainerNamePrefix = "https_server";
  static constexpr std::string_view kReadyMessage = "Starting HTTPS service";
};

class Go1_16_TLSClientContainer : public ContainerRunner {
 public:
  Go1_16_TLSClientContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_https/client/golang_1_16_https_client.tar";
  static constexpr std::string_view kContainerNamePrefix = "https_client";
  static constexpr std::string_view kReadyMessage = R"({"status":"ok"})";
};

class Go1_17_TLSServerContainer : public ContainerRunner {
 public:
  Go1_17_TLSServerContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_https/server/golang_1_17_https_server.tar";
  static constexpr std::string_view kContainerNamePrefix = "https_server";
  static constexpr std::string_view kReadyMessage = "Starting HTTPS service";
};

class Go1_17_TLSClientContainer : public ContainerRunner {
 public:
  Go1_17_TLSClientContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_https/client/golang_1_17_https_client.tar";
  static constexpr std::string_view kContainerNamePrefix = "https_client";
  static constexpr std::string_view kReadyMessage = R"({"status":"ok"})";
};

//-----------------------------------------------------------------------------
// GRPC
//-----------------------------------------------------------------------------

class Go1_16_GRPCServerContainer : public ContainerRunner {
 public:
  Go1_16_GRPCServerContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_grpc_tls_pl/server/golang_1_16_grpc_tls_server.tar";
  static constexpr std::string_view kContainerNamePrefix = "grpc_server";
  static constexpr std::string_view kReadyMessage = "Starting HTTP/2 server";
};

class Go1_16_GRPCClientContainer : public ContainerRunner {
 public:
  Go1_16_GRPCClientContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_grpc_tls_pl/client/golang_1_16_grpc_tls_client.tar";
  static constexpr std::string_view kContainerNamePrefix = "grpc_client";
  static constexpr std::string_view kReadyMessage = "";
};

class Go1_17_GRPCServerContainer : public ContainerRunner {
 public:
  Go1_17_GRPCServerContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_grpc_tls_pl/server/golang_1_17_grpc_tls_server.tar";
  static constexpr std::string_view kContainerNamePrefix = "grpc_server";
  static constexpr std::string_view kReadyMessage = "Starting HTTP/2 server";
};

class Go1_17_GRPCClientContainer : public ContainerRunner {
 public:
  Go1_17_GRPCClientContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_grpc_tls_pl/client/golang_1_17_grpc_tls_client.tar";
  static constexpr std::string_view kContainerNamePrefix = "grpc_client";
  static constexpr std::string_view kReadyMessage = "";
};

//-----------------------------------------------------------------------------
// DNS
//-----------------------------------------------------------------------------

// A DNS server using the bind9 DNS server image.
class DNSServerContainer : public ContainerRunner {
 public:
  DNSServerContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/dns_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "dns_server";
  static constexpr std::string_view kReadyMessage = "all zones loaded";
};

//-----------------------------------------------------------------------------
// Mux
//-----------------------------------------------------------------------------

class ThriftMuxServerContainer : public ContainerRunner {
 public:
  ThriftMuxServerContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux/server_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "thriftmux_server";
  static constexpr std::string_view kReadyMessage = "Finagle version";
};

//-----------------------------------------------------------------------------
// MySQL
//-----------------------------------------------------------------------------

class MySQLContainer : public ContainerRunner {
 public:
  MySQLContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/mysql_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "mysql_server";
  static constexpr std::string_view kReadyMessage =
      "/usr/sbin/mysqld: ready for connections. Version: '8.0.13'  socket: "
      "'/var/lib/mysql/mysql.sock'  port: 3306";
};

class PythonMySQLConnectorContainer : public ContainerRunner {
 public:
  PythonMySQLConnectorContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/mysql_connector_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "mysql_client";
  static constexpr std::string_view kReadyMessage = "pid";
};

//-----------------------------------------------------------------------------
// Postgres
//-----------------------------------------------------------------------------

class PostgreSQLContainer : public ContainerRunner {
 public:
  PostgreSQLContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/"
      "postgres_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "postgres_testing";
  static constexpr std::string_view kReadyMessage =
      "database system is ready to accept connections";
};

class GolangSQLxContainer : public ContainerRunner {
 public:
  GolangSQLxContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/pgsql/demo_client_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "pgsql_demo";
  static constexpr std::string_view kReadyMessage = "";
};

//-----------------------------------------------------------------------------
// Kafka
//-----------------------------------------------------------------------------

class KafkaContainer : public ContainerRunner {
 public:
  KafkaContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/kafka_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "kafka_server";
  static constexpr std::string_view kReadyMessage =
      "Recorded new controller, from now on will use broker";
};

class ZooKeeperContainer : public ContainerRunner {
 public:
  ZooKeeperContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/zookeeper_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "zookeeper_server";
  static constexpr std::string_view kReadyMessage = "INFO PrepRequestProcessor (sid:0) started";
};

//-----------------------------------------------------------------------------
// Redis
//-----------------------------------------------------------------------------

class RedisContainer : public ContainerRunner {
 public:
  RedisContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/redis_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "redis_test";
  static constexpr std::string_view kReadyMessage = "# Server initialized";
};

//-----------------------------------------------------------------------------
// Cassandra
//-----------------------------------------------------------------------------

class CassandraContainer : public ContainerRunner {
 public:
  CassandraContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/"
      "datastax_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "dse_server";
  static constexpr std::string_view kReadyMessage = "DSE startup complete.";
};

//-----------------------------------------------------------------------------
// NATS
//-----------------------------------------------------------------------------

class NATSServerContainer : public ContainerRunner {
 public:
  NATSServerContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/nats_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "nats_server";
  static constexpr std::string_view kReadyMessage = "Server is ready";
};

class NATSClientContainer : public ContainerRunner {
 public:
  NATSClientContainer()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/protocols/nats/testing/"
      "nats_test_client_with_ca_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "nats_test_client";
  static constexpr std::string_view kReadyMessage = "";
};

//-----------------------------------------------------------------------------
// Sock-shop (for GRPC testing)
//-----------------------------------------------------------------------------

class ProductCatalogService : public ContainerRunner {
 public:
  ProductCatalogService() : ContainerRunner(kImage, kContainerNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kImage =
      "gcr.io/google-samples/microservices-demo/productcatalogservice:v0.2.0";
  static constexpr std::string_view kContainerNamePrefix = "pcs";
  static constexpr std::string_view kReadyMessage = "starting grpc server";
};

class ProductCatalogClient : public ContainerRunner {
 public:
  ProductCatalogClient()
      : ContainerRunner(::px::testing::BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/hipster_shop/productcatalogservice_client/"
      "productcatalogservice_client_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "pcc";
  static constexpr std::string_view kReadyMessage = "";
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
