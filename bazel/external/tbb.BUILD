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

licenses(["notice"])  # 3-Clause BSD

exports_files(["LICENSE"])

# Taken from: https://github.com/rnburn/satyr/blob/master/bazel/tbb.BUILD
# License for this BUILD file: MIT
# See: https://github.com/rnburn/satyr/blob/master/LICENSE
#
# License for TBB: Apache 2.0
# See: https://github.com/01org/tbb/blob/tbb_2018/LICENSE
genrule(
    name = "build_tbb",
    srcs = glob(["**"]) + ["@local_config_cc//:toolchain"],
    outs = [
        "libtbb.a",
        "libtbbmalloc.a",
    ],
    cmd = """
        set -e
        # Checks to see if second arg is absolute, if so returns that. If not
        # prepends the first arg to it.
        FixPath() {
            pwd=$$1
            tool_path=$$2
            if [[ "$$tool_path" = /* ]]
            then
                echo "$$tool_path"
            else
                echo "$$pwd/$$tool_path"
            fi
        }
        WORK_DIR=$$PWD
        DEST_DIR=$$PWD/$(@D)
        export PATH=$$PATH
        export CC=$$CC
        export CXX=$$CXX
        export CXXFLAGS="-O3 -Wno-deprecated-copy"
        cd $$(dirname $(location :Makefile))
        COMPILER_OPT="compiler=clang"

        # uses extra_inc=big_iron.inc to specify that static libraries are
        # built. See https://software.intel.com/en-us/forums/intel-threading-building-blocks/topic/297792
        make -j $$(nproc) tbb_build_prefix="build" \
              extra_inc=big_iron.inc \
              $$COMPILER_OPT 2>&1 > make.out || cat make.out
        cp build/build_release/*.a $$DEST_DIR
        cd $$WORK_DIR
    """,
)

cc_library(
    name = "tbb",
    srcs = ["libtbb.a"],
    hdrs = glob([
        "include/serial/**",
        "include/tbb/**/**",
    ]),
    includes = ["include"],
    visibility = ["//visibility:public"],
)
