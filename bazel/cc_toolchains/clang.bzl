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

load("@rules_cc//cc:defs.bzl", "cc_toolchain")
load("@unix_cc_toolchain_config//:cc_toolchain_config.bzl", "cc_toolchain_config")

def _clang_x86_64_gnu():
    _clang_x86_64_gnu_with_options(
        extra_target_constraints = [
            ":is_exec_false",
        ],
    )

def _clang_exec():
    _clang_x86_64_gnu_with_options(
        suffix = "-exec",
        enable_sanitizers = False,
        extra_target_constraints = [
            ":is_exec_true",
        ],
    )

def _clang_x86_64_gnu_with_options(suffix = "", enable_sanitizers = True, extra_target_constraints = []):
    toolchain_config_name = "clang_config_x86_64_gnu" + suffix
    toolchain_identifier = "clang-x86_64-linux-gnu" + suffix
    cc_toolchain_name = "cc-compiler-clang-x86_64-gnu" + suffix
    toolchain_name = "cc-toolchain-clang-x86_64-gnu" + suffix
    tool_paths = {
        "ar": "/opt/clang-15.0/bin/llvm-ar",
        "cpp": "/opt/clang-15.0/bin/clang-cpp",
        "dwp": "/opt/clang-15.0/bin/llvm-dwp",
        "gcc": "/opt/clang-15.0/bin/clang-15",
        "ld": "/opt/clang-15.0/bin/ld.lld",
        "llvm-cov": "/opt/clang-15.0/bin/llvm-cov",
        "nm": "/opt/clang-15.0/bin/llvm-nm",
        "objcopy": "/opt/clang-15.0/bin/llvm-objcopy",
        "objdump": "/opt/clang-15.0/bin/llvm-objdump",
        "strip": "/opt/clang-15.0/bin/llvm-strip",
    }
    cc_toolchain_config(
        name = toolchain_config_name,
        cpu = "k8",
        compiler = "clang",
        toolchain_identifier = toolchain_identifier,
        host_system_name = "x86_64-unknown-linux-gnu",
        target_system_name = "x86_64-unknown-linux-gnu",
        target_libc = "glibc_unknown",
        abi_version = "clang",
        abi_libc_version = "glibc_unknown",
        cxx_builtin_include_directories = [
            "/opt/clang-15.0/lib/clang/15.0.6/include",
            "/usr/local/include",
            "/usr/include/x86_64-linux-gnu",
            "/usr/include",
            "/opt/clang-15.0/lib/clang/15.0.6/share",
            "/usr/include/c++/11",
            "/usr/include/x86_64-linux-gnu/c++/11",
            "/usr/include/c++/11/backward",
            "/opt/clang-15.0/include/c++/v1",
        ],
        tool_paths = tool_paths,
        compile_flags = [
            "-fstack-protector",
            "-Wall",
            "-Wthread-safety",
            "-Wself-assign",
            "-Wunused-but-set-parameter",
            "-fcolor-diagnostics",
            "-fno-omit-frame-pointer",
        ],
        opt_compile_flags = [
            "-g0",
            "-O2",
            "-D_FORTIFY_SOURCE=1",
            "-DNDEBUG",
            "-ffunction-sections",
            "-fdata-sections",
        ],
        dbg_compile_flags = ["-g"],
        cxx_flags = [
            "-std=c++17",
            "-fPIC",
        ],
        link_flags = [
            "-static-libgcc",
            "-fuse-ld=lld",
            "-Wl,-no-as-needed",
            "-Wl,-z,relro,-z,now",
            "-B/opt/clang-15.0/bin",
            "-lm",
        ],
        opt_link_flags = ["-Wl,--gc-sections"],
        unfiltered_compile_flags = [
            "-no-canonical-prefixes",
            "-Wno-builtin-macro-redefined",
            "-D__DATE__=\"redacted\"",
            "-D__TIMESTAMP__=\"redacted\"",
            "-D__TIME__=\"redacted\"",
        ],
        coverage_compile_flags = ["--coverage"],
        coverage_link_flags = ["--coverage"],
        supports_start_end_lib = True,
        libclang_rt_path = "/opt/clang-15.0/lib/clang/15.0.6/lib/linux",
        enable_sanitizers = enable_sanitizers,
    )

    cc_toolchain(
        name = cc_toolchain_name,
        toolchain_identifier = toolchain_identifier,
        toolchain_config = toolchain_config_name,
        # TODO(james): figure out what these files values do, and if we need them.
        all_files = ":empty",
        ar_files = ":empty",
        as_files = ":empty",
        compiler_files = ":empty",
        dwp_files = ":empty",
        linker_files = ":empty",
        objcopy_files = ":empty",
        strip_files = ":empty",
        supports_param_files = 1,
        module_map = None,
    )

    native.toolchain(
        name = toolchain_name,
        exec_compatible_with = [
            "@platforms//cpu:x86_64",
            "@platforms//os:linux",
        ],
        target_compatible_with = [
            "@platforms//cpu:x86_64",
            "@platforms//os:linux",
        ] + extra_target_constraints,
        target_settings = [
            ":compiler_clang",
            ":libc_version_gnu",
        ],
        toolchain = ":" + cc_toolchain_name,
        toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
    )

clang_x86_64_gnu = _clang_x86_64_gnu
clang_exec = _clang_exec
