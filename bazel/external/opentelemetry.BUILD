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
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "common_proto",
    srcs = [
        "opentelemetry/proto/common/v1/common.proto",
    ],
)

cc_proto_library(
    name = "common_proto_cc",
    deps = [":common_proto"],
)

proto_library(
    name = "resource_proto",
    srcs = [
        "opentelemetry/proto/resource/v1/resource.proto",
    ],
    deps = [
        ":common_proto",
    ],
)

cc_proto_library(
    name = "resource_proto_cc",
    deps = [":resource_proto"],
)

proto_library(
    name = "trace_proto",
    srcs = [
        "opentelemetry/proto/trace/v1/trace.proto",
    ],
    deps = [
        ":common_proto",
        ":resource_proto",
    ],
)

cc_proto_library(
    name = "trace_proto_cc",
    deps = [":trace_proto"],
)

proto_library(
    name = "trace_service_proto",
    srcs = [
        "opentelemetry/proto/collector/trace/v1/trace_service.proto",
    ],
    deps = [
        ":trace_proto",
    ],
)

cc_proto_library(
    name = "trace_service_proto_cc",
    deps = [":trace_service_proto"],
)

cc_grpc_library(
    name = "trace_service_grpc_cc",
    srcs = [":trace_service_proto"],
    generate_mocks = True,
    grpc_only = True,
    deps = [":trace_service_proto_cc"],
)

proto_library(
    name = "metrics_proto",
    srcs = [
        "opentelemetry/proto/metrics/v1/metrics.proto",
    ],
    deps = [
        ":common_proto",
        ":resource_proto",
    ],
)

cc_proto_library(
    name = "metrics_proto_cc",
    deps = [":metrics_proto"],
)

proto_library(
    name = "metrics_service_proto",
    srcs = [
        "opentelemetry/proto/collector/metrics/v1/metrics_service.proto",
    ],
    deps = [
        ":metrics_proto",
    ],
)

cc_proto_library(
    name = "metrics_service_proto_cc",
    deps = [":metrics_service_proto"],
)

cc_grpc_library(
    name = "metrics_service_grpc_cc",
    srcs = [":metrics_service_proto"],
    generate_mocks = True,
    grpc_only = True,
    deps = [":metrics_service_proto_cc"],
)
