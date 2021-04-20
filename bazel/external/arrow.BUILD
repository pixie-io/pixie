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

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "arrow",
    # This list is copied from ARROW_SRCS in
    # https://github.com/apache/arrow/blob/master/cpp/src/arrow/CMakeLists.txt.
    # When updating Arrow, make sure to update this list to ensure that any new files are included.
    #
    # Also make sure to re-generate the following generated files, and include them in the repo.
    #    cpp/src/arrow/ipc/File_generated.h
    #    cpp/src/arrow/ipc/Message_generated.h
    #    cpp/src/arrow/ipc/Schema_generated.h
    #    cpp/src/arrow/ipc/Tensor_generated.h
    #    cpp/src/arrow/ipc/feather_generated.h
    # Try "cmake .; make arrow_dependencies" to do this.
    # Apologies for the hack!
    srcs = [
        "cpp/src/arrow/array.cc",
        "cpp/src/arrow/array/builder_adaptive.cc",
        "cpp/src/arrow/array/builder_base.cc",
        "cpp/src/arrow/array/builder_binary.cc",
        "cpp/src/arrow/array/builder_decimal.cc",
        "cpp/src/arrow/array/builder_dict.cc",
        "cpp/src/arrow/array/builder_nested.cc",
        "cpp/src/arrow/array/builder_primitive.cc",
        "cpp/src/arrow/array/builder_union.cc",
        "cpp/src/arrow/array/concatenate.cc",
        "cpp/src/arrow/buffer.cc",
        "cpp/src/arrow/builder.cc",
        "cpp/src/arrow/compare.cc",
        "cpp/src/arrow/extension_type.cc",
        "cpp/src/arrow/memory_pool.cc",
        "cpp/src/arrow/pretty_print.cc",
        "cpp/src/arrow/record_batch.cc",
        "cpp/src/arrow/result.cc",
        "cpp/src/arrow/scalar.cc",
        "cpp/src/arrow/sparse_tensor.cc",
        "cpp/src/arrow/status.cc",
        "cpp/src/arrow/table.cc",
        "cpp/src/arrow/table_builder.cc",
        "cpp/src/arrow/tensor.cc",
        "cpp/src/arrow/testing/util.cc",
        "cpp/src/arrow/type.cc",
        "cpp/src/arrow/util/basic_decimal.cc",
        "cpp/src/arrow/util/bit-util.cc",
        "cpp/src/arrow/util/compression.cc",
        "cpp/src/arrow/util/cpu-info.cc",
        "cpp/src/arrow/util/decimal.cc",
        "cpp/src/arrow/util/int-util.cc",
        "cpp/src/arrow/util/key_value_metadata.cc",
        "cpp/src/arrow/util/logging.cc",
        "cpp/src/arrow/util/memory.cc",
        "cpp/src/arrow/util/string_builder.cc",
        "cpp/src/arrow/util/task-group.cc",
        "cpp/src/arrow/util/thread-pool.cc",
        "cpp/src/arrow/util/trie.cc",
        "cpp/src/arrow/util/utf8.cc",
        "cpp/src/arrow/util/util.cc",
        "cpp/src/arrow/vendored/datetime/tz.cpp",
        "cpp/src/arrow/visitor.cc",
    ],
    hdrs = glob(
        [
            "cpp/src/arrow/*.h",
            "cpp/src/arrow/**/*.h",
            "cpp/src/arrow/*.hpp",
            "cpp/src/arrow/**/*.hpp",
        ],
        exclude = ["cpp/src/arrow/io/hdfs-internal.h"],
    ),
    copts = [
        "-Wno-unused-parameter",
        "-Wno-overloaded-virtual",
        "-Wno-deprecated-declarations",
    ],
    includes = [
        "cpp/src",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_tencent_rapidjson//:rapidjson",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/strings",
        "@com_google_flatbuffers//:flatbuffers",
    ],
)
