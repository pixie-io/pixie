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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_jar")

def fetch_netty_tcnative_jars(version):
    http_jar(
        name = "netty_tcnative_boringssl_static",
        url = "https://repo1.maven.org/maven2/io/netty/netty-tcnative-boringssl-static/%s/netty-tcnative-boringssl-static-%s.jar" % (version, version),
        sha256 = "0d8d16adadb19e065a5ac05738f0c2503c685cf3edafba14f7a1c246aafa09ef",
    )

    http_jar(
        name = "netty_tcnative_boringssl_static_linux_x86_64",
        url = "https://repo1.maven.org/maven2/io/netty/netty-tcnative-boringssl-static/%s/netty-tcnative-boringssl-static-%s-linux-x86_64.jar" % (version, version),
        sha256 = "83e3357da5567a93cb5ff6cb807d70573359d7ef9676fa8169405121bae05723",
    )

    http_jar(
        name = "netty_tcnative_boringssl_static_linux_aarch_64",
        url = "https://repo1.maven.org/maven2/io/netty/netty-tcnative-boringssl-static/%s/netty-tcnative-boringssl-static-%s-linux-aarch_64.jar" % (version, version),
        sha256 = "ce3d12e3ae2ad0b9225df347b55715b0ad24342d0195bb238f5e3f60b3f6b868",
    )
