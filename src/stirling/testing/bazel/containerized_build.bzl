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

load("@io_bazel_rules_docker//container:container.bzl", "container_image")
load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_commit", "container_run_and_extract")

# A rule for building auxiliary go binaries used in tests.
# These builds are performed in a container, decoupling them from our go toolchain.
# This lets us control the go version with which to build the auxiliary test binaries,
# so we can ensure Stirling works on different versions of Go.
# It also provides more determinism in our tests (e.g. less churn on go toolchain upgrades).
# Main outputs:
#  <name>: The stand-alone binary
#  <name>_image_with_binary_commit.tar: A container with the built binary in the CWD.
def pl_aux_go_binary(name, files, base, build_flags = ""):
    # Build path within the binary where the sources will be placed and built.
    container_build_dir = "/go/src/" + name
    outfile = container_build_dir + "/" + name

    container_image(
        name = name + "_image_with_source",
        base = base,
        directory = container_build_dir,
        files = files,
    )

    container_run_and_commit(
        name = name + "_image_with_binary",
        commands = [
            "go mod init",
            "go get",
            "go build -v " + build_flags,
        ],
        docker_run_flags = ["-w " + container_build_dir],
        image = ":" + name + "_image_with_source.tar",
    )

    container_run_and_extract(
        name = name + "_extractor",
        commands = ["echo"],
        extract_file = outfile,
        image = ":" + name + "_image_with_binary_commit.tar",
    )

    native.genrule(
        name = name + "_gen",
        outs = [name],
        srcs = [":" + name + "_extractor" + outfile],
        cmd = "cp $< $@",
    )
