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

# When adding an image here, first add it to scripts/regclient/regbot_deps.yaml
# Once that is in, trigger the github workflow that mirrors the required image
# to the various registries we use.
# Finally the image may be added here and used subsequently.

# Switch this to another mirror if the selected registry is down or inaccessible
selected_registry = "ghcr.io"

namespaced_mirrors = {
    "docker.io": "docker.io/pixie-io",
    "ghcr.io": "ghcr.io/pixie-io",
    "quay.io": "quay.io/pixie",
}

def _container_image(name, digest, repository):
    namespace = namespaced_mirrors[selected_registry].removeprefix(selected_registry + "/")
    image_repo = namespace + "/" + repository.replace("/", "-")

    container_pull(
        name = name,
        digest = digest,
        registry = selected_registry,
        repository = image_repo,
    )

def base_images():
    # Based on alpine 3.15.9, using OpenResty 1.21.4.1
    # https://hub.docker.com/layers/openresty/openresty/alpine-apk-amd64/images/sha256-2259f28de01f85c22e32b6964254a4551c54a1d554cd4b5f1615d7497e1a09ce?context=explore
    _container_image(
        name = "openresty",
        repository = "openresty/openresty",
        digest = "sha256:2259f28de01f85c22e32b6964254a4551c54a1d554cd4b5f1615d7497e1a09ce",
    )

    _container_image(
        name = "base_image",
        repository = "distroless/base",
        digest = "sha256:8267a5d9fa15a538227a8850e81cf6c548a78de73458e99a67e8799bbffb1ba0",
    )

    _container_image(
        name = "base_image_debug",
        repository = "distroless/base",
        digest = "sha256:c59a1e5509d1b2586e28b899667774e599b79d7289a6bb893766a0cbbce7384b",
    )

def stirling_test_jdk_images():
    # https://hub.docker.com/layers/zulu-openjdk/azul/zulu-openjdk/18/images/sha256-01a1519ff66c3038e4c66f36e5dcf4dbc68278058d83133c0bc942518fcbef6e?context=explore
    # azul/zulu-openjdk:18
    _container_image(
        name = "azul-zulu",
        repository = "azul/zulu-openjdk",
        digest = "sha256:01a1519ff66c3038e4c66f36e5dcf4dbc68278058d83133c0bc942518fcbef6e",
    )

    # https://hub.docker.com/layers/zulu-openjdk-debian/azul/zulu-openjdk-debian/15.0.6/images/sha256-d9df673eae28bd1c8e23fabcbd6c76d20285ea7e5c589975bd26866cab189c2a?context=explore
    # azul/zulu-openjdk-debian:15.0.6
    _container_image(
        name = "azul-zulu-debian",
        repository = "azul/zulu-openjdk-debian",
        digest = "sha256:d9df673eae28bd1c8e23fabcbd6c76d20285ea7e5c589975bd26866cab189c2a",
    )

    # https://hub.docker.com/layers/zulu-openjdk-alpine/azul/zulu-openjdk-alpine/17-jre/images/sha256-eef2da2a134370717e40b1cc570efba08896520af6b31744eabf64481a986878?context=explore
    # azul/zulu-openjdk-alpine:17-jre
    _container_image(
        name = "azul-zulu-alpine",
        repository = "azul/zulu-openjdk-alpine",
        digest = "sha256:eef2da2a134370717e40b1cc570efba08896520af6b31744eabf64481a986878",
    )

    # https://hub.docker.com/layers/amazoncorretto/library/amazoncorretto/18-alpine-jdk/images/sha256-52679264dee28c1cbe2ff8455efc86cc44cbceb6f94d9971abd7cd7e4c8bdc50?context=explore
    # amazoncorretto:18-alpine-jdk
    _container_image(
        name = "amazon-corretto",
        repository = "amazoncorretto",
        digest = "sha256:52679264dee28c1cbe2ff8455efc86cc44cbceb6f94d9971abd7cd7e4c8bdc50",
    )

    # https://hub.docker.com/layers/adoptopenjdk/library/adoptopenjdk/openj9/images/sha256-2b739b781a601a9d1e5a98fb3d47fe9dcdbd989e92c4e4eb4743364da67ca05e?context=explore
    # adoptopenjdk:openj9
    _container_image(
        name = "adopt-j9",
        repository = "adoptopenjdk",
        digest = "sha256:2b739b781a601a9d1e5a98fb3d47fe9dcdbd989e92c4e4eb4743364da67ca05e",
    )

    # https://hub.docker.com/layers/ibmjava/library/ibmjava/8-jre/images/sha256-78e2dd462373b3c5631183cc927a54aef1b114c56fe2fb3e31c4b39ba2d919dc?context=explore
    # ibmjava:8-jre
    _container_image(
        name = "ibm",
        repository = "ibmjava",
        digest = "sha256:78e2dd462373b3c5631183cc927a54aef1b114c56fe2fb3e31c4b39ba2d919dc",
    )

    # https://hub.docker.com/layers/sapmachine/library/sapmachine/18.0.1/images/sha256-53a036f4d787126777c010437ee4802de11b193e8aca556170301ab2c2359bc6?context=explore
    # sapmachine:18.0.1
    _container_image(
        name = "sap",
        repository = "sapmachine",
        digest = "sha256:53a036f4d787126777c010437ee4802de11b193e8aca556170301ab2c2359bc6",
    )

    _container_image(
        name = "graal-vm",
        repository = "graalvm/jdk",
        digest = "sha256:ffb117a5fd76d8c47120e1b4186053c306ae850483b59f24a5979d7154d35685",
    )

def stirling_test_images():
    stirling_test_jdk_images()

    # NGINX with OpenSSL 1.1.0, for OpenSSL tracing tests.
    _container_image(
        name = "nginx_openssl_1_1_0_base_image",
        repository = "nginx",
        digest = "sha256:204a9a8e65061b10b92ad361dd6f406248404fe60efd5d6a8f2595f18bb37aad",
    )

    # NGINX with OpenSSL 1.1.1, for OpenSSL tracing tests.
    # nginx:1.23.2-alpine
    _container_image(
        name = "nginx_openssl_1_1_1_base_image",
        repository = "nginx",
        digest = "sha256:0f2ab24c6aba5d96fcf6e7a736333f26dca1acf5fa8def4c276f6efc7d56251f",
    )

    # NGINX with OpenSSL 3.0.8, for OpenSSL tracing tests.
    # nginx:1.23.3-alpine-slim
    _container_image(
        name = "nginx_alpine_openssl_3_0_8_base_image",
        repository = "nginx",
        digest = "sha256:3eb380b81387e9f2a49cb6e5e18db016e33d62c37ea0e9be2339e9f0b3e26170",
    )

    # DNS server image for DNS tests.
    _container_image(
        name = "alpine_dns_base_image",
        repository = "resystit/bind9",
        digest = "sha256:b9d834c7ca1b3c0fb32faedc786f2cb96fa2ec00976827e3f0c44f647375e18c",
    )

    # Curl container, for OpenSSL tracing tests.
    # curlimages/curl:7.74.0
    _container_image(
        name = "curl_base_image",
        repository = "curlimages/curl",
        digest = "sha256:5594e102d5da87f8a3a6b16e5e9b0e40292b5404c12f9b6962fd6b056d2a4f82",
    )

    # Ruby container, for OpenSSL tracing tests.
    # ruby:3.0.0-slim-buster
    _container_image(
        name = "ruby_base_image",
        repository = "ruby",
        digest = "sha256:47eeeb05f545b5a7d18a84c16d585ed572d38379ebb7106061f800f5d9abeb38",
    )

    # Datastax DSE server, for CQL tracing tests.
    # datastax/dse-server:6.7.7
    _container_image(
        name = "datastax_base_image",
        repository = "datastax/dse-server",
        digest = "sha256:a98e1a877f9c1601aa6dac958d00e57c3f6eaa4b48d4f7cac3218643a4bfb36e",
    )

    # Postgres server, for PGSQL tracing tests.
    # postgres:13.2-alpine
    _container_image(
        name = "postgres_base_image",
        repository = "postgres",
        digest = "sha256:3335d0494b62ae52f0c18a1e4176a83991c9d3727fe67d8b1907b569de2f6175",
    )

    # Redis server, for Redis tracing tests.
    # redis:6.2.1
    _container_image(
        name = "redis_base_image",
        repository = "redis",
        digest = "sha256:fd68bec9c2cdb05d74882a7eb44f39e1c6a59b479617e49df245239bba4649f9",
    )

    # MySQL server, for MySQL tracing tests.
    # mysql/mysql-server:8.0.13
    _container_image(
        name = "mysql_base_image",
        repository = "mysql/mysql-server",
        digest = "sha256:3d50c733cc42cbef715740ed7b4683a8226e61911e3a80c3ed8a30c2fbd78e9a",
    )

    # Custom-built container with python MySQL client, for MySQL tests.
    _container_image(
        name = "python_mysql_connector_image",
        repository = "python_mysql_connector",
        digest = "sha256:ae7fb76afe1ab7c34e2d31c351579ee340c019670559716fd671126e85894452",
    )

    # Custom built nats:2.9.19-scratch with dwarf info
    _container_image(
        name = "nats_base_image",
        repository = "nats",
        digest = "sha256:55521ffe36911fb4edeaeecb7f9219f9d2a09bc275530212b89e41ab78a7f16d",
    )

    # Kafka broker image, for testing.
    _container_image(
        name = "kafka_base_image",
        repository = "confluentinc/cp-kafka",
        digest = "sha256:ee6e42ce4f79623c69cf758848de6761c74bf9712697fe68d96291a2b655ce7f",
    )

    # Zookeeper image for Kafka.
    _container_image(
        name = "zookeeper_base_image",
        repository = "confluentinc/cp-zookeeper",
        digest = "sha256:87314e87320abf190f0407bf1689f4827661fbb4d671a41cba62673b45b66bfa",
    )

    # Tag: node:12.3.1-stretch-slim
    # Arch: linux/amd64
    _container_image(
        name = "node_12_3_1_linux_amd64_image",
        repository = "node",
        digest = "sha256:86acb148010d15bc63200493000bb8fc70ea052f7da2a80d34d7741110378eb3",
    )

    # Tag: node:14.18.1-alpine
    # Arch: linux/amd64
    _container_image(
        name = "node_14_18_1_alpine_linux_amd64_image",
        repository = "node",
        digest = "sha256:1b50792b5ed9f78fe08f24fbf57334cc810410af3861c5c748de055186bf082c",
    )

    # Tag: rabbitmq:3-management
    # Arch: linux/amd64
    _container_image(
        name = "rabbitmq_3_management",
        repository = "rabbitmq",
        digest = "sha256:650c7e0093842739ddfaadec0d45946c052dba42941bd5c0a082cbe914451c25",
    )

    # Tag: productcatalogservice:v0.2.0
    # Arch: linux/amd64
    _container_image(
        name = "productcatalogservice_v0_2_0",
        repository = "google-samples/microservices-demo/productcatalogservice",
        digest = "sha256:1726e4dd813190ad1eae7f3c42483a3a83dd1676832bb7b04256455c8968d82a",
    )

    # Built and pushed by src/stirling/testing/demo_apps/py_grpc/update_gcr.sh
    _container_image(
        name = "py_grpc_helloworld_image",
        repository = "python_grpc_1_19_0_helloworld",
        digest = "sha256:e04fc4e9b10508eed74c4154cb1f96d047dc0195b6ef0c9d4a38d6e24238778e",
    )

    _container_image(
        name = "emailservice_image",
        repository = "google-samples/microservices-demo/emailservice",
        digest = "sha256:d42ee712cbb4806a8b922e303a5e6734f342dfb6c92c81284a289912165b7314",
    )

    # Tag: mongo:7.0
    # Arch: linux/amd64
    _container_image(
        name = "mongo_7_0",
        repository = "mongo",
        digest = "sha256:19b2e5c91f92c7b18113a1501c5a5fe52b71a6c6d2a5232eeebb4f2abacae04a",
    )
