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

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
load("@com_github_bazelbuild_buildtools//buildifier:deps.bzl", "buildifier_dependencies")
load("@distroless//package_manager:dpkg.bzl", "dpkg_list", "dpkg_src")
load("@distroless//package_manager:package_manager.bzl", "package_manager_repositories")
load("@io_bazel_rules_docker//container:container.bzl", "container_pull")
load("@io_bazel_rules_docker//go:image.bzl", _go_image_repos = "repositories")
load("@io_bazel_rules_docker//java:image.bzl", _java_image_repos = "repositories")
load("@io_bazel_rules_docker//repositories:deps.bzl", container_deps = "deps")
load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)
load("@io_bazel_rules_k8s//k8s:k8s.bzl", "k8s_repositories")
load("@io_bazel_rules_k8s//k8s:k8s_go_deps.bzl", k8s_go_deps = "deps")

# Sets up package manager which we use build deploy images.
def _package_manager_setup():
    package_manager_repositories()

    dpkg_src(
        name = "debian_sid",
        arch = "amd64",
        distro = "sid",
        sha256 = "a093727908ebb7e46cc83643b21d5e81eadee49efaa3ecae7aa7ff1a62858396",
        snapshot = "20200701T101354Z",
        url = "http://snapshot.debian.org/archive",
    )

    dpkg_list(
        name = "package_bundle",
        packages = [
            "libc6",
            "libelf1",
            "liblzma5",
            "libtinfo6",
            "zlib1g",
            "libsasl2-2",
            "libssl1.1",
            "libgcc1",
        ],
        sources = ["@debian_sid//file:Packages.json"],
    )

# Set-up images used by Stirling BPF tests.
def stirling_docker_images_setup():
    # Busybox container, for basic container tests.
    container_pull(
        name = "distroless_base_image",
        digest = "sha256:f989df6099c5efb498021c7f01b74f484b46d2f5e1cdb862e508569d87569f2b",
        registry = "gcr.io",
        repository = "distroless/base",
    )

    # NGINX with OpenSSL 1.1.0, for OpenSSL tracing tests.
    container_pull(
        name = "nginx_openssl_1_1_0_base_image",
        digest = "sha256:204a9a8e65061b10b92ad361dd6f406248404fe60efd5d6a8f2595f18bb37aad",
        registry = "index.docker.io",
        repository = "library/nginx",
    )

    # NGINX with OpenSSL 1.1.1, for OpenSSL tracing tests.
    container_pull(
        name = "nginx_openssl_1_1_1_base_image",
        digest = "sha256:0b159cd1ee1203dad901967ac55eee18c24da84ba3be384690304be93538bea8",
        registry = "index.docker.io",
        repository = "library/nginx",
    )

    # DNS server image for DNS tests.
    container_pull(
        name = "alpine_dns_base_image",
        digest = "sha256:b9d834c7ca1b3c0fb32faedc786f2cb96fa2ec00976827e3f0c44f647375e18c",
        registry = "index.docker.io",
        repository = "resystit/bind9",
    )

    # Curl container, for OpenSSL tracing tests.
    # curlimages/curl:7.74.0
    container_pull(
        name = "curl_base_image",
        digest = "sha256:5594e102d5da87f8a3a6b16e5e9b0e40292b5404c12f9b6962fd6b056d2a4f82",
        registry = "index.docker.io",
        repository = "curlimages/curl",
    )

    # Ruby container, for OpenSSL tracing tests.
    # ruby:3.0.0-buster
    container_pull(
        name = "ruby_base_image",
        digest = "sha256:beeed8e63b1ae4a1492f4be9cd40edc6bdb1009b94228438f162d0d05e10c8fd",
        registry = "index.docker.io",
        repository = "library/ruby",
    )

    # Datastax DSE server, for CQL tracing tests.
    # datastax/dse-server:6.7.7
    container_pull(
        name = "datastax_base_image",
        digest = "sha256:a98e1a877f9c1601aa6dac958d00e57c3f6eaa4b48d4f7cac3218643a4bfb36e",
        registry = "index.docker.io",
        repository = "datastax/dse-server",
    )

    # Postgres server, for PGSQL tracing tests.
    # postgres:13.2
    container_pull(
        name = "postgres_base_image",
        digest = "sha256:661dc59f4a71e689c51d4823963baa56b8fcc8daa5b16cf740cad236fa5ffe74",
        registry = "index.docker.io",
        repository = "library/postgres",
    )

    # Redis server, for Redis tracing tests.
    # redis:6.2.1
    container_pull(
        name = "redis_base_image",
        digest = "sha256:fd68bec9c2cdb05d74882a7eb44f39e1c6a59b479617e49df245239bba4649f9",
        registry = "index.docker.io",
        repository = "library/redis",
    )

    # MySQL server, for MySQL tracing tests.
    # mysql/mysql-server:8.0.13
    container_pull(
        name = "mysql_base_image",
        digest = "sha256:3d50c733cc42cbef715740ed7b4683a8226e61911e3a80c3ed8a30c2fbd78e9a",
        registry = "index.docker.io",
        repository = "mysql/mysql-server",
    )

    # Custom-built container with python MySQL client, for MySQL tests.
    container_pull(
        name = "python_mysql_connector_image",
        digest = "sha256:fe281dfaaf51dd00bdc4b6f8064c22138cfbc656d44372cd71b97449d790d241",
        registry = "gcr.io",
        repository = "pixie-oss/pixie-dev-public/python_mysql_connector",
    )

    # NATS server image, for testing.
    container_pull(
        name = "nats_base_image",
        digest = "sha256:da01747b3b1b793d26f23d641b1e96439c8a4a7e06f996382ad5a588b3f22e9a",
        registry = "index.docker.io",
        repository = "library/nats",
    )

def _docker_images_setup():
    _go_image_repos()
    _java_image_repos()

    # Import NGINX repo.
    container_pull(
        name = "nginx_base",
        digest = "sha256:204a9a8e65061b10b92ad361dd6f406248404fe60efd5d6a8f2595f18bb37aad",
        registry = "index.docker.io",
        repository = "library/nginx",
    )

    container_pull(
        name = "openresty",
        # Stretch image.
        digest = "sha256:1702786dcbb5b6b6d096f5e56b2153d8b508e62396fd4324367913b6645bb0b8",
        registry = "index.docker.io",
        repository = "openresty/openresty",
    )

    container_pull(
        name = "base_image",
        digest = "sha256:e37cf3289c1332c5123cbf419a1657c8dad0811f2f8572433b668e13747718f8",
        registry = "gcr.io",
        repository = "distroless/base",
    )

    container_pull(
        name = "base_image_debug",
        digest = "sha256:f989df6099c5efb498021c7f01b74f484b46d2f5e1cdb862e508569d87569f2b",
        registry = "gcr.io",
        repository = "distroless/base",
    )

    stirling_docker_images_setup()

def _artifacts_setup():
    http_file(
        name = "linux_headers_4_14_176_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-4.14.176-trimmed-pl3.tar.gz"],
        sha256 = "08d4daeba7b4c81454be8cdd3d3da65b3bcae8fedf0f3787690c85310bf91664",
        downloaded_file_path = "linux-headers-4.14.176.tar.gz",
    )

    http_file(
        name = "linux_headers_4_15_18_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-4.15.18-trimmed-pl3.tar.gz"],
        sha256 = "4d8ba1fdfa2f085a32457a1f2656c2c9cfc75fa5410d508579d4c6c06cf3ee2f",
        downloaded_file_path = "linux-headers-4.15.18.tar.gz",
    )

    http_file(
        name = "linux_headers_4_16_18_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-4.16.18-trimmed-pl3.tar.gz"],
        sha256 = "ff41877f051f67653801e10e546e5e29d98fde9cc2ec7981a7217ba853f5ab33",
        downloaded_file_path = "linux-headers-4.16.18.tar.gz",
    )

    http_file(
        name = "linux_headers_4_17_19_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-4.17.19-trimmed-pl3.tar.gz"],
        sha256 = "b4924b84ca3703d788eb7962b3df30fa13788978326d8f9f3a2913da6154b0a0",
        downloaded_file_path = "linux-headers-4.17.19.tar.gz",
    )

    http_file(
        name = "linux_headers_4_18_20_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-4.18.20-trimmed-pl3.tar.gz"],
        sha256 = "7260e6e7c18baaaa9f49913a1c756ab431b03ad8caa0ca15b34bfaedb232509a",
        downloaded_file_path = "linux-headers-4.18.20.tar.gz",
    )

    http_file(
        name = "linux_headers_4_19_118_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-4.19.118-trimmed-pl3.tar.gz"],
        sha256 = "bb41b959b34483f9206157e3195b7567c594d7595d3fdef94d579932684aed39",
        downloaded_file_path = "linux-headers-4.19.118.tar.gz",
    )

    http_file(
        name = "linux_headers_4_20_17_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-4.20.17-trimmed-pl3.tar.gz"],
        sha256 = "8024a42ead5dd474aa8190896f070df7c4c08c77228b1f18143779aa0ab877c5",
        downloaded_file_path = "linux-headers-4.20.17.tar.gz",
    )

    http_file(
        name = "linux_headers_5_0_21_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.0.21-trimmed-pl3.tar.gz"],
        sha256 = "3d044deec38127c05de3951cc5521370f500e8b85faed0497c447356dbd3af49",
        downloaded_file_path = "linux-headers-5.0.21.tar.gz",
    )

    http_file(
        name = "linux_headers_5_1_21_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.1.21-trimmed-pl3.tar.gz"],
        sha256 = "0f1669f5100c3f3f1ccc448401ba7ef88388ae08ede540b23df3744b9e81d344",
        downloaded_file_path = "linux-headers-5.1.21.tar.gz",
    )

    http_file(
        name = "linux_headers_5_2_21_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.2.21-trimmed-pl3.tar.gz"],
        sha256 = "31207779809d6e3b118ca6dd00bde382744d8ff05f53918658f21ac1bac6a423",
        downloaded_file_path = "linux-headers-5.2.21.tar.gz",
    )

    http_file(
        name = "linux_headers_5_3_18_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.3.18-trimmed-pl3.tar.gz"],
        sha256 = "ddac47a719244aee65f18e0d05d80f063b8d28db54258243590a27509d691753",
        downloaded_file_path = "linux-headers-5.3.18.tar.gz",
    )

    http_file(
        name = "linux_headers_5_4_35_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.4.35-trimmed-pl3.tar.gz"],
        sha256 = "b8eec62c5638642c4eb44f99cce5bf6862d3958e566733250c5e7aa4db2ccb13",
        downloaded_file_path = "linux-headers-5.4.35.tar.gz",
    )

    http_file(
        name = "linux_headers_5_5_19_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.5.19-trimmed-pl3.tar.gz"],
        sha256 = "3a8e2eded3645cbab6e495246616dbe0700bc9d39359cbf7859d402fcb339e34",
        downloaded_file_path = "linux-headers-5.5.19.tar.gz",
    )

    http_file(
        name = "linux_headers_5_6_19_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.6.19-trimmed-pl3.tar.gz"],
        sha256 = "ca3485b6f61696efbaf748f1edc9f307b9a0c7b208b545145277687f97e7e6f5",
        downloaded_file_path = "linux-headers-5.6.19.tar.gz",
    )

    http_file(
        name = "linux_headers_5_7_19_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.7.19-trimmed-pl3.tar.gz"],
        sha256 = "40a1484c3af86f7ef6e9f7bc497f2fdf6a9e052f0d80cff58f40f6dd4f53d158",
        downloaded_file_path = "linux-headers-5.7.19.tar.gz",
    )

    http_file(
        name = "linux_headers_5_8_18_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.8.18-trimmed-pl3.tar.gz"],
        sha256 = "fc7f5ac376e3af8b16f608dec7da5ce95e3dc78b87a172d545597e2800e5e7bf",
        downloaded_file_path = "linux-headers-5.8.18.tar.gz",
    )

    http_file(
        name = "linux_headers_5_9_16_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.9.16-trimmed-pl3.tar.gz"],
        sha256 = "2d68890188ff4b20c28704cda7e774afdd4729b15a2376f8677105d4851459f5",
        downloaded_file_path = "linux-headers-5.9.16.tar.gz",
    )

    http_file(
        name = "linux_headers_5_10_34_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.10.34-trimmed-pl3.tar.gz"],
        sha256 = "2dcf53abec0f49bffa5b5ceb65465c6d41ce620d304666955d37cd0f1d9e53f5",
        downloaded_file_path = "linux-headers-5.10.34.tar.gz",
    )

    http_file(
        name = "linux_headers_5_11_18_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers-5.11.18-trimmed-pl3.tar.gz"],
        sha256 = "1ba2d5db17e2c913e8e0f0f99be35a83ffba89dd10cfe9d8051d9d8f3b7a75e4",
        downloaded_file_path = "linux-headers-5.11.18.tar.gz",
    )

    http_file(
        name = "timeconst_100",
        urls = ["https://storage.googleapis.com/pixie-dev-public/timeconst_100.h"],
        sha256 = "082496c45ab93af811732da56000caf5ffc9e6734ff633a2b348291f160ceb7e",
        downloaded_file_path = "timeconst_100.h",
    )

    http_file(
        name = "timeconst_250",
        urls = ["https://storage.googleapis.com/pixie-dev-public/timeconst_250.h"],
        sha256 = "0db01d74b846e39dca3612d96dee8b8f6addfaeb738cc4f5574086828487c2b9",
        downloaded_file_path = "timeconst_250.h",
    )

    http_file(
        name = "timeconst_300",
        urls = ["https://storage.googleapis.com/pixie-dev-public/timeconst_300.h"],
        sha256 = "91c6499df71695699a296b2fdcbb8c30e9bf35d024e048fa6d2305a8ac2af9ab",
        downloaded_file_path = "timeconst_300.h",
    )

    http_file(
        name = "timeconst_1000",
        urls = ["https://storage.googleapis.com/pixie-dev-public/timeconst_1000.h"],
        sha256 = "da0ba6765f2969482bf8eaf21249552557fe4d6831749d9cfe4c25f4661f8726",
        downloaded_file_path = "timeconst_1000.h",
    )

# TODO(zasgar): remove this when downstream bugs relying on bazel version are removed.
def _impl(repository_ctx):
    bazel_verision_for_upb = "bazel_version = \"" + native.bazel_version + "\""
    bazel_version_for_foreign_cc = "BAZEL_VERSION = \"" + native.bazel_version + "\""
    repository_ctx.file("bazel_version.bzl", bazel_verision_for_upb)
    repository_ctx.file("def.bzl", bazel_version_for_foreign_cc)
    repository_ctx.file("BUILD", "")

bazel_version_repository = repository_rule(
    implementation = _impl,
    local = True,
)

def pl_workspace_setup():
    gazelle_dependencies()
    buildifier_dependencies()

    bazel_version_repository(
        name = "bazel_version",
    )

    container_repositories()
    container_deps()

    k8s_repositories()
    k8s_go_deps()

def pl_docker_images():
    _package_manager_setup()
    _docker_images_setup()
    _artifacts_setup()
