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

# IMPORTANT: NEVER use docker hub images directly.
# Docker now has a rate limit on image pulls that can prevent builds from running.
# Instead pull the image you need from docker hub and push it to
# gcr.io/pixie-oss/pixie-dev-public/docker-deps/<your-image-name-here>

# Although, ghcr doesn't currently have rate limits to avoid potential future issues
# and to standardize, we also do the same for ghcr images.
# Pull any image you need from ghcr and push it to
# gcr.io/pixie-oss/pixie-dev-public/ghcr-deps/<your-image-name-here>

def _gcr_io_image(name, digest, repo):
    container_pull(
        name = name,
        digest = digest,
        registry = "gcr.io",
        repository = repo,
    )

def base_images():
    _gcr_io_image(
        "nginx_base",
        "sha256:204a9a8e65061b10b92ad361dd6f406248404fe60efd5d6a8f2595f18bb37aad",
        "pixie-oss/pixie-dev-public/docker-deps/library/nginx",
    )

    _gcr_io_image(
        # This should be 1.21.4.1-7 but that version hasn't been tagged yet. So picking the versionless build.
        # 1.21.4.1-6 uses alpine:3.15.7 with some open CVEs.
        # As of writing this, alpine-apk-amd64 maps to a future 1.21.4.1-7 release based on alpine:3.15.8 with no known CVEs.
        # https://hub.docker.com/layers/openresty/openresty/alpine-apk-amd64/images/sha256-eac84c6c543d2424e07e980b7aff897a4cb2463d79977e579f6285f27d8e8d99?context=explore
        "openresty",
        "sha256:eac84c6c543d2424e07e980b7aff897a4cb2463d79977e579f6285f27d8e8d99",
        "pixie-oss/pixie-dev-public/docker-deps/openresty/openresty",
    )

    _gcr_io_image(
        "base_image",
        "sha256:8267a5d9fa15a538227a8850e81cf6c548a78de73458e99a67e8799bbffb1ba0",
        "distroless/base",
    )

    _gcr_io_image(
        "base_image_debug",
        "sha256:c59a1e5509d1b2586e28b899667774e599b79d7289a6bb893766a0cbbce7384b",
        "distroless/base",
    )

    _gcr_io_image(
        "openjdk-base-glibc",
        "sha256:d7048f5a32ca7598f583c492c960496848cc9017fdb55942370f02603c83561d",
        "pixie-oss/pixie-dev-public/docker-deps/library/openjdk",
    )

    _gcr_io_image(
        "openjdk-base-musl",
        "sha256:25b910311bfe15547ecab6895d5eb3f4ec718d6d53cced7eec78e4b889962e1f",
        "pixie-oss/pixie-dev-public/docker-deps/library/openjdk",
    )

    _gcr_io_image(
        # https://hub.docker.com/layers/zulu-openjdk/azul/zulu-openjdk/18/images/sha256-01a1519ff66c3038e4c66f36e5dcf4dbc68278058d83133c0bc942518fcbef6e?context=explore
        # azul/zulu-openjdk:18
        "azul-zulu",
        "sha256:01a1519ff66c3038e4c66f36e5dcf4dbc68278058d83133c0bc942518fcbef6e",
        "pixie-oss/pixie-dev-public/docker-deps/azul/zulu-openjdk",
    )

    _gcr_io_image(
        # https://hub.docker.com/layers/zulu-openjdk-debian/azul/zulu-openjdk-debian/15.0.6/images/sha256-d9df673eae28bd1c8e23fabcbd6c76d20285ea7e5c589975bd26866cab189c2a?context=explore
        # azul/zulu-openjdk-debian:15.0.6
        "azul-zulu-debian",
        "sha256:d9df673eae28bd1c8e23fabcbd6c76d20285ea7e5c589975bd26866cab189c2a",
        "pixie-oss/pixie-dev-public/docker-deps/azul/zulu-openjdk-debian",
    )

    _gcr_io_image(
        # https://hub.docker.com/layers/zulu-openjdk-alpine/azul/zulu-openjdk-alpine/17-jre/images/sha256-eef2da2a134370717e40b1cc570efba08896520af6b31744eabf64481a986878?context=explore
        # azul/zulu-openjdk-alpine:17-jre
        "azul-zulu-alpine",
        "sha256:eef2da2a134370717e40b1cc570efba08896520af6b31744eabf64481a986878",
        "pixie-oss/pixie-dev-public/docker-deps/azul/zulu-openjdk-alpine",
    )

    _gcr_io_image(
        # https://hub.docker.com/layers/amazoncorretto/library/amazoncorretto/18-alpine-jdk/images/sha256-52679264dee28c1cbe2ff8455efc86cc44cbceb6f94d9971abd7cd7e4c8bdc50?context=explore
        # amazoncorretto:18-alpine-jdk
        "amazon-corretto",
        "sha256:52679264dee28c1cbe2ff8455efc86cc44cbceb6f94d9971abd7cd7e4c8bdc50",
        "pixie-oss/pixie-dev-public/docker-deps/library/amazoncorretto",
    )

    _gcr_io_image(
        # https://hub.docker.com/layers/adoptopenjdk/library/adoptopenjdk/openj9/images/sha256-2b739b781a601a9d1e5a98fb3d47fe9dcdbd989e92c4e4eb4743364da67ca05e?context=explore
        # adoptopenjdk:openj9
        "adopt-j9",
        "sha256:2b739b781a601a9d1e5a98fb3d47fe9dcdbd989e92c4e4eb4743364da67ca05e",
        "pixie-oss/pixie-dev-public/docker-deps/amd64/adoptopenjdk",
    )

    _gcr_io_image(
        # https://hub.docker.com/layers/ibmjava/library/ibmjava/8-jre/images/sha256-78e2dd462373b3c5631183cc927a54aef1b114c56fe2fb3e31c4b39ba2d919dc?context=explore
        # ibmjava:8-jre
        "ibm",
        "sha256:78e2dd462373b3c5631183cc927a54aef1b114c56fe2fb3e31c4b39ba2d919dc",
        "pixie-oss/pixie-dev-public/docker-deps/library/ibmjava",
    )

    _gcr_io_image(
        # https://hub.docker.com/layers/sapmachine/library/sapmachine/18.0.1/images/sha256-53a036f4d787126777c010437ee4802de11b193e8aca556170301ab2c2359bc6?context=explore
        # sapmachine:18.0.1
        "sap",
        "sha256:53a036f4d787126777c010437ee4802de11b193e8aca556170301ab2c2359bc6",
        "pixie-oss/pixie-dev-public/docker-deps/library/sapmachine",
    )

    _gcr_io_image(
        "graal-vm",
        "sha256:2a1aa528041342d76074a82ea72ad962d661eba68a6eceb100dc2e66e7377d55",
        "pixie-oss/pixie-dev-public/ghcr-deps/graalvm/jdk",
    )

    _gcr_io_image(
        "graal-vm-ce",
        "sha256:f7b8643f448dd9e108cda69cd42f8c86681710a18fbc64392d8c62126a3e1651",
        "pixie-oss/pixie-dev-public/ghcr-deps/graalvm/graalvm-ce",
    )

def stirling_test_images():
    # NGINX with OpenSSL 1.1.0, for OpenSSL tracing tests.
    _gcr_io_image(
        "nginx_openssl_1_1_0_base_image",
        "sha256:204a9a8e65061b10b92ad361dd6f406248404fe60efd5d6a8f2595f18bb37aad",
        "pixie-oss/pixie-dev-public/docker-deps/library/nginx",
    )

    # NGINX with OpenSSL 1.1.1, for OpenSSL tracing tests.
    _gcr_io_image(
        "nginx_openssl_1_1_1_base_image",
        "sha256:0b159cd1ee1203dad901967ac55eee18c24da84ba3be384690304be93538bea8",
        "pixie-oss/pixie-dev-public/docker-deps/library/nginx",
    )

    _gcr_io_image(
        "nginx_alpine_openssl_3_0_7_base_image",
        "sha256:3eb380b81387e9f2a49cb6e5e18db016e33d62c37ea0e9be2339e9f0b3e26170",
        "pixie-oss/pixie-dev-public/docker-deps/library/nginx",
    )

    # DNS server image for DNS tests.
    _gcr_io_image(
        "alpine_dns_base_image",
        "sha256:b9d834c7ca1b3c0fb32faedc786f2cb96fa2ec00976827e3f0c44f647375e18c",
        "pixie-oss/pixie-dev-public/docker-deps/resystit/bind9",
    )

    # Curl container, for OpenSSL tracing tests.
    # curlimages/curl:7.74.0
    _gcr_io_image(
        "curl_base_image",
        "sha256:c50aa334d3dc674cb87bbd6ab3dd42c92ff1cfac47df9d65bb78efc8f990d716",
        "pixie-oss/pixie-dev-public/docker-deps/curlimages/curl",
    )

    # Ruby container, for OpenSSL tracing tests.
    # ruby:3.0.0-buster
    _gcr_io_image(
        "ruby_base_image",
        "sha256:beeed8e63b1ae4a1492f4be9cd40edc6bdb1009b94228438f162d0d05e10c8fd",
        "pixie-oss/pixie-dev-public/docker-deps/library/ruby",
    )

    # Datastax DSE server, for CQL tracing tests.
    # datastax/dse-server:6.7.7
    _gcr_io_image(
        "datastax_base_image",
        "sha256:a98e1a877f9c1601aa6dac958d00e57c3f6eaa4b48d4f7cac3218643a4bfb36e",
        "pixie-oss/pixie-dev-public/docker-deps/datastax/dse-server",
    )

    # Postgres server, for PGSQL tracing tests.
    # postgres:13.2
    _gcr_io_image(
        "postgres_base_image",
        "sha256:661dc59f4a71e689c51d4823963baa56b8fcc8daa5b16cf740cad236fa5ffe74",
        "pixie-oss/pixie-dev-public/docker-deps/library/postgres",
    )

    # Redis server, for Redis tracing tests.
    # redis:6.2.1
    _gcr_io_image(
        "redis_base_image",
        "sha256:fd68bec9c2cdb05d74882a7eb44f39e1c6a59b479617e49df245239bba4649f9",
        "pixie-oss/pixie-dev-public/docker-deps/library/redis",
    )

    # MySQL server, for MySQL tracing tests.
    # mysql/mysql-server:8.0.13
    _gcr_io_image(
        "mysql_base_image",
        "sha256:3d50c733cc42cbef715740ed7b4683a8226e61911e3a80c3ed8a30c2fbd78e9a",
        "pixie-oss/pixie-dev-public/docker-deps/mysql/mysql-server",
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
    _gcr_io_image(
        "kafka_base_image",
        "sha256:ee6e42ce4f79623c69cf758848de6761c74bf9712697fe68d96291a2b655ce7f",
        "pixie-oss/pixie-dev-public/docker-deps/confluentinc/cp-kafka",
    )

    # Zookeeper image for Kafka.
    _gcr_io_image(
        "zookeeper_base_image",
        "sha256:87314e87320abf190f0407bf1689f4827661fbb4d671a41cba62673b45b66bfa",
        "pixie-oss/pixie-dev-public/docker-deps/confluentinc/cp-zookeeper",
    )

    # Tag: node:12.3.1
    # Arch: linux/amd64
    # This is the oldest tag on docker hub that can be pulled. Older tags cannot be pulled because
    # of server error on docker hub, which presumably is because of they are too old.
    _gcr_io_image(
        "node_12_3_1_linux_amd64_image",
        "sha256:ade8d367d98b5074a8c3a4e2d74bd657b578d4a500090d66c2da33801ec4b58d",
        "pixie-oss/pixie-dev-public/docker-deps/node",
    )

    # Tag: node:14.18.1-alpine
    # Arch: linux/amd64
    _gcr_io_image(
        "node_14_18_1_alpine_amd64_image",
        "sha256:1b50792b5ed9f78fe08f24fbf57334cc810410af3861c5c748de055186bf082c",
        "pixie-oss/pixie-dev-public/docker-deps/node",
    )

    # Tag: node:16.9
    # Arch: linux/amd64
    _gcr_io_image(
        "node_16_9_linux_amd64_image",
        "sha256:b0616a801a0f3c17c437c67c49e20c76c8735e205cdc165e56ae4fa867f32af1",
        "pixie-oss/pixie-dev-public/docker-deps/node",
    )

    # Tag: rabbitmq:3-management
    # Arch: linux/amd64
    _gcr_io_image(
        "rabbitmq_3_management",
        "sha256:650c7e0093842739ddfaadec0d45946c052dba42941bd5c0a082cbe914451c25",
        "pixie-oss/demo-apps/rabbitmq/rabbitmq:3-management",
    )

    # Tag: productcatalogservice:v0.2.0
    # Arch: linux/amd64
    _gcr_io_image(
        "productcatalogservice_v0_2_0",
        "sha256:1726e4dd813190ad1eae7f3c42483a3a83dd1676832bb7b04256455c8968d82a",
        "google-samples/microservices-demo/productcatalogservice:v0.2.0",
    )

    # Built and pushed by src/stirling/testing/demo_apps/py_grpc/update_gcr.sh
    _gcr_io_image(
        "py_grpc_helloworld_image",
        "sha256:e04fc4e9b10508eed74c4154cb1f96d047dc0195b6ef0c9d4a38d6e24238778e",
        "pixie-oss/pixie-dev-public/python_grpc_1_19_0_helloworld:1.2",
    )

    _gcr_io_image(
        "emailservice_image",
        "sha256:d42ee712cbb4806a8b922e303a5e6734f342dfb6c92c81284a289912165b7314",
        "google-samples/microservices-demo/emailservice:v0.1.3",
    )
