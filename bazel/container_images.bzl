# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

load("@io_bazel_rules_docker//container:container.bzl", "container_pull")

def _docker_io_image(name, digest, repo):
    container_pull(
        name = name,
        digest = digest,
        registry = "index.docker.io",
        repository = repo,
    )

def _gcr_io_image(name, digest, repo):
    container_pull(
        name = name,
        digest = digest,
        registry = "gcr.io",
        repository = repo,
    )

def base_images():
    _docker_io_image(
        "nginx_base",
        "sha256:204a9a8e65061b10b92ad361dd6f406248404fe60efd5d6a8f2595f18bb37aad",
        "library/nginx",
    )

    _docker_io_image(
        "openresty",
        "sha256:1702786dcbb5b6b6d096f5e56b2153d8b508e62396fd4324367913b6645bb0b8",
        "openresty/openresty",
    )

    _gcr_io_image(
        "base_image",
        "sha256:e37cf3289c1332c5123cbf419a1657c8dad0811f2f8572433b668e13747718f8",
        "distroless/base",
    )

    _gcr_io_image(
        "base_image_debug",
        "sha256:f989df6099c5efb498021c7f01b74f484b46d2f5e1cdb862e508569d87569f2b",
        "distroless/base",
    )

def stirling_test_images():
    # NGINX with OpenSSL 1.1.0, for OpenSSL tracing tests.
    _docker_io_image(
        "nginx_openssl_1_1_0_base_image",
        "sha256:204a9a8e65061b10b92ad361dd6f406248404fe60efd5d6a8f2595f18bb37aad",
        "library/nginx",
    )

    # NGINX with OpenSSL 1.1.1, for OpenSSL tracing tests.
    _docker_io_image(
        "nginx_openssl_1_1_1_base_image",
        "sha256:0b159cd1ee1203dad901967ac55eee18c24da84ba3be384690304be93538bea8",
        "library/nginx",
    )

    # DNS server image for DNS tests.
    _docker_io_image(
        "alpine_dns_base_image",
        "sha256:b9d834c7ca1b3c0fb32faedc786f2cb96fa2ec00976827e3f0c44f647375e18c",
        "resystit/bind9",
    )

    # Curl container, for OpenSSL tracing tests.
    # curlimages/curl:7.74.0
    _docker_io_image(
        "curl_base_image",
        "sha256:5594e102d5da87f8a3a6b16e5e9b0e40292b5404c12f9b6962fd6b056d2a4f82",
        "curlimages/curl",
    )

    # Ruby container, for OpenSSL tracing tests.
    # ruby:3.0.0-buster
    _docker_io_image(
        "ruby_base_image",
        "sha256:beeed8e63b1ae4a1492f4be9cd40edc6bdb1009b94228438f162d0d05e10c8fd",
        "library/ruby",
    )

    # Datastax DSE server, for CQL tracing tests.
    # datastax/dse-server:6.7.7
    _docker_io_image(
        "datastax_base_image",
        "sha256:a98e1a877f9c1601aa6dac958d00e57c3f6eaa4b48d4f7cac3218643a4bfb36e",
        "datastax/dse-server",
    )

    # Postgres server, for PGSQL tracing tests.
    # postgres:13.2
    _docker_io_image(
        "postgres_base_image",
        "sha256:661dc59f4a71e689c51d4823963baa56b8fcc8daa5b16cf740cad236fa5ffe74",
        "library/postgres",
    )

    # Redis server, for Redis tracing tests.
    # redis:6.2.1
    _docker_io_image(
        "redis_base_image",
        "sha256:fd68bec9c2cdb05d74882a7eb44f39e1c6a59b479617e49df245239bba4649f9",
        "library/redis",
    )

    # MySQL server, for MySQL tracing tests.
    # mysql/mysql-server:8.0.13
    _docker_io_image(
        "mysql_base_image",
        "sha256:3d50c733cc42cbef715740ed7b4683a8226e61911e3a80c3ed8a30c2fbd78e9a",
        "mysql/mysql-server",
    )

    # Custom-built container with python MySQL client, for MySQL tests.
    _gcr_io_image(
        "python_mysql_connector_image",
        "sha256:ae7fb76afe1ab7c34e2d31c351579ee340c019670559716fd671126e85894452",
        "pixie-oss/pixie-dev-public/python_mysql_connector",
    )

    # NATS server image, for testing. This isn't the official image. The difference is that this
    # includes symbols in the executable.
    _gcr_io_image(
        "nats_base_image",
        "sha256:93179975b83acaf1ff7581e9e23c59d838e780599a80f795ae90e97de08c4aae",
        "pixie-oss/pixie-dev-public/nats/nats-server",
    )

    # Kafka broker image, for testing.
    _docker_io_image(
        "kafka_base_image",
        "sha256:ee6e42ce4f79623c69cf758848de6761c74bf9712697fe68d96291a2b655ce7f",
        "confluentinc/cp-kafka",
    )

    # Zookeeper image for Kafka.
    _docker_io_image(
        "zookeeper_base_image",
        "sha256:87314e87320abf190f0407bf1689f4827661fbb4d671a41cba62673b45b66bfa",
        "confluentinc/cp-zookeeper",
    )

    # Tag: node:12.3.1
    # Arch: linux/amd64
    # This is the oldest tag on docker hub that can be pulled. Older tags cannot be pulled because
    # of server error on docker hub, which presumably is because of they are too old.
    _docker_io_image(
        "node_12_3_1_linux_amd64_image",
        "sha256:ade8d367d98b5074a8c3a4e2d74bd657b578d4a500090d66c2da33801ec4b58d",
        "node",
    )

    # Tag: node:14.18.1-alpine
    # Arch: linux/amd64
    _docker_io_image(
        "node_14_18_1_alpine_amd64_image",
        "sha256:1b50792b5ed9f78fe08f24fbf57334cc810410af3861c5c748de055186bf082c",
        "node",
    )

    # Tag: node:16.9
    # Arch: linux/amd64
    _docker_io_image(
        "node_16_9_linux_amd64_image",
        "sha256:b0616a801a0f3c17c437c67c49e20c76c8735e205cdc165e56ae4fa867f32af1",
        "node",
    )
