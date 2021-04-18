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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

proto_library(
    name = "base_proto",
    srcs = ["src/sentencepiece.proto"],
    strip_import_prefix = "src",
)

cc_proto_library(
    name = "base_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":base_proto"],
)

proto_library(
    name = "model_proto",
    srcs = ["src/sentencepiece_model.proto"],
    strip_import_prefix = "src",
)

cc_proto_library(
    name = "model_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":model_proto"],
)

cc_library(
    name = "thirdparty",
    srcs = [],
    hdrs = [
        "third_party/darts_clone/darts.h",
        "third_party/esaxx/esa.hxx",
    ],
)

cc_library(
    name = "libsentencepiece_internal",
    srcs = [
        "src/bpe_model.cc",
        "src/char_model.cc",
        "src/error.cc",
        "src/filesystem.cc",
        "src/model_factory.cc",
        "src/model_interface.cc",
        "src/normalizer.cc",
        "src/sentencepiece_processor.cc",
        "src/unigram_model.cc",
        "src/util.cc",
        "src/word_model.cc",
    ],
    hdrs = [
        "src/bpe_model.h",
        "src/char_model.h",
        "src/common.h",
        "src/filesystem.h",
        "src/freelist.h",
        "src/init.h",
        "src/model_factory.h",
        "src/model_interface.h",
        "src/normalizer.h",
        "src/sentencepiece_processor.h",
        "src/testharness.h",
        "src/unigram_model.h",
        "src/util.h",
        "src/word_model.h",
    ],
    strip_include_prefix = "src",
    deps = [
        ":base_cc_proto",
        ":model_cc_proto",
        ":thirdparty",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "libsentencepiece",
    srcs = [],
    hdrs = [
        "src/sentencepiece_processor.h",
    ],
    include_prefix = "sentencepiece",
    strip_include_prefix = "src",
    visibility = ["//visibility:public"],
    deps = [":libsentencepiece_internal"],
)
