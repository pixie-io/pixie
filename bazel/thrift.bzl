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

load("@io_bazel_rules_scala//twitter_scrooge:twitter_scrooge.bzl", "twitter_scrooge")
load("@rules_jvm_external//:defs.bzl", "maven_install")

def thrift_deps(scala_version):
    twitter_scrooge()

    finagle_version = "22.4.0"
    scala_minor_version = ".".join(scala_version.split(".")[:2])

    maven_install(
        artifacts = [
            # "com.twitter:finagle-thriftmux_%s:%s" % (scala_minor_version, finagle_version),
            # "com.twitter:finagle-mux_%s:%s" % (scala_minor_version, finagle_version),
            # "com.twitter:finagle-core_%s:%s" % (scala_minor_version, finagle_version),
            "com.twitter:scrooge-core_%s:%s" % (scala_minor_version, finagle_version),
            "com.twitter:scrooge-generator_%s:%s" % (scala_minor_version, finagle_version),
            "com.twitter:finagle-http_%s:%s" % (scala_minor_version, finagle_version),
            "org.apache.thrift:libthrift:0.10.0",
            "org.slf4j:slf4j-api:1.7.36",
            "ch.qos.logback:logback-core:1.2.10",
            "ch.qos.logback:logback-classic:1.2.10",
        ],
        repositories = ["https://repo1.maven.org/maven2"],
        # All of these jars will be provided from building finagle internally to work around the fact
        # that a newer version has not been released since netty and netty-tcnative were released.
        excluded_artifacts = [
            "finagle-core_%s:%s" % (scala_minor_version, finagle_version),
            "finagle-toggle_%s:%s" % (scala_minor_version, finagle_version),
            "finagle-init_%s:%s" % (scala_minor_version, finagle_version),
            "finagle-http_%s:%s" % (scala_minor_version, finagle_version),
            "finagle-http2_%s:%s" % (scala_minor_version, finagle_version),
            "finagle-netty4-http_%s:%s" % (scala_minor_version, finagle_version),
            "finagle-netty4_%s:%s" % (scala_minor_version, finagle_version),
            "finagle-base_%s:%s" % (scala_minor_version, finagle_version),
            "finagle-mux_%s:%s" % (scala_minor_version, finagle_version),
            "finagle-thrift_%s:%s" % (scala_minor_version, finagle_version),
            "finagle-partitioning_%s:%s" % (scala_minor_version, finagle_version),
            "finagle-thriftmux_%s:%s" % (scala_minor_version, finagle_version),

            "io.netty:netty-handler",
            "io.netty:netty-handler-proxy",
            "io.netty:netty-common",
            "io.netty:netty-transport",
            "io.netty:netty-transport-native-epoll",
            "io.netty:netty-transport-classes-epoll",
            "io.netty:netty-transport-native-unix-common",
            "io.netty:netty-buffer",
            "io.netty:netty-resolver",
            "io.netty:netty-codec-http2",
            "io.netty:netty-codec",
            "io.netty:netty-codec-http",
            "io.netty:netty-codec-socks",
            "io.netty:netty-tcnative-boringssl-static",
            "io.netty:netty-tcnative-classes",
        ],
    )
