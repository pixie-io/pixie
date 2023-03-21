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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
load("@com_github_bazelbuild_buildtools//buildifier:deps.bzl", "buildifier_dependencies")
load("@io_bazel_rules_docker//go:image.bzl", _go_image_repos = "repositories")
load("@io_bazel_rules_docker//java:image.bzl", _java_image_repos = "repositories")
load("@io_bazel_rules_docker//repositories:deps.bzl", container_deps = "deps")
load("@io_bazel_rules_docker//repositories:repositories.bzl", container_repositories = "repositories")
load("@io_bazel_rules_docker//scala:image.bzl", _scala_image_repos = "repositories")
load("@io_bazel_rules_k8s//k8s:k8s.bzl", "k8s_repositories")
load("@io_bazel_rules_k8s//k8s:k8s_go_deps.bzl", k8s_go_deps = "deps")
load("//bazel:container_images.bzl", "base_images", "stirling_test_images")
load("//bazel:linux_headers.bzl", "linux_headers")
load("//bazel:pl_qemu_kernels.bzl", "qemu_kernel_deps")
load("//bazel/external/ubuntu_packages:packages.bzl", "download_ubuntu_packages")

# Sets up package manager which we use build deploy images.
def _package_manager_setup():
    download_ubuntu_packages()

def _container_images_setup():
    _go_image_repos()
    _java_image_repos()
    _scala_image_repos()
    base_images()
    stirling_test_images()

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
    buildifier_dependencies()

    bazel_version_repository(name = "bazel_version")

    container_repositories()
    container_deps()

    k8s_repositories()
    k8s_go_deps(go_version = None)

    qemu_with_kernel_deps()

def pl_container_images():
    _package_manager_setup()
    _container_images_setup()
    linux_headers()

def pl_model_files():
    # Download model files
    http_file(
        name = "embedding_model",
        url = "https://storage.googleapis.com/pixie-dev-public/ml-data/models/current-embedding-model.proto",
        downloaded_file_path = "embedding.proto",
        sha256 = "a23c515c139670e71c0cad5c962f7e2d968fcc57ab251e49f4b5636134628813",
    )

    http_file(
        name = "sentencepiece_model",
        url = "https://storage.googleapis.com/pixie-dev-public/ml-data/models/current-sentencepiece-model.proto",
        downloaded_file_path = "sentencepiece.proto",
        sha256 = "7e17e04ecc207d9204dc8755357f988bf77c135f7a34a88984943c8649d6a790",
    )

def qemu_with_kernel_deps():
    qemu_kernel_deps()

    http_file(
        name = "busybox",
        url = "https://storage.googleapis.com/pixie-dev-public/mirrors/busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox",
        sha256 = "6e123e7f3202a8c1e9b1f94d8941580a25135382b99e8d3e34fb858bba311348",
        downloaded_file_path = "busybox",
        executable = True,
    )
