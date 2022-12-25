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

def _gcc_x86_64_gnu():
    tool_paths = {
        "ar": "/usr/bin/ar",
        "cpp": "/usr/bin/cpp",
        "dwp": "/usr/bin/dwp",
        "gcc": "/usr/bin/gcc-11",
        "gcov": "/usr/bin/gcov",
        "ld": "/usr/bin/ld",
        "llvm-cov": "/opt/clang-15.0/bin/llvm-cov",
        "nm": "/usr/bin/nm",
        "objcopy": "/usr/bin/objcopy",
        "objdump": "/usr/bin/objdump",
        "strip": "/usr/bin/strip",
    }

    cc_toolchain_config(
        name = "gcc_config_x84_64_gnu",
        cpu = "k8",
        compiler = "gcc",
        toolchain_identifier = "gcc-x86_64-linux-gnu",
        host_system_name = "x86_64-unknown-linux-gnu",
        target_system_name = "x86_64-unknown-linux-gnu",
        target_libc = "glibc_unknown",
        abi_version = "gcc",
        abi_libc_version = "glibc_unknown",
        cxx_builtin_include_directories = [
            "/usr/lib/gcc/x86_64-linux-gnu/11/include",
            "/usr/local/include",
            "/usr/include/x86_64-linux-gnu",
            "/usr/include",
            "/usr/include/c++/11",
            "/usr/include/x86_64-linux-gnu/c++/11",
            "/usr/include/c++/11/backward",
        ],
        tool_paths = tool_paths,
        compile_flags = [
            "-fstack-protector",
            "-Wall",
            "-Wunused-but-set-parameter",
            "-Wno-free-nonheap-object",
            "-fno-omit-frame-pointer",
        ],
        cxx_flags = ["-std=c++17"],
        opt_compile_flags = [
            "-g0",
            "-O2",
            "-D_FORTIFY_SOURCE=1",
            "-DNDEBUG",
            "-ffunction-sections",
            "-fdata-sections",
        ],
        dbg_compile_flags = ["-g"],
        link_flags = [
            "-fuse-ld=lld",
            "-Wl,-no-as-needed",
            "-Wl,-z,relro,-z,now",
            "-B/usr/bin",
            "-pass-exit-codes",
            "-lm",
        ],
        opt_link_flags = ["-Wl,--gc-sections"],
        unfiltered_compile_flags = [
            "-fno-canonical-system-headers",
            "-Wno-builtin-macro-redefined",
            "-D__DATE__=\"redacted\"",
            "-D__TIMESTAMP__=\"redacted\"",
            "-D__TIME__=\"redacted\"",
        ],
        coverage_compile_flags = ["--coverage"],
        coverage_link_flags = ["--coverage"],
        supports_start_end_lib = True,
    )

    cc_toolchain(
        name = "cc-compiler-gcc-x86_64-gnu",
        toolchain_identifier = "gcc-x86_64-linux-gnu",
        toolchain_config = "gcc_config_x84_64_gnu",
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
        name = "cc-toolchain-gcc-x86_64-gnu",
        exec_compatible_with = [
            "@platforms//cpu:x86_64",
            "@platforms//os:linux",
        ],
        target_compatible_with = [
            "@platforms//cpu:x86_64",
            "@platforms//os:linux",
        ],
        target_settings = [
            ":compiler_gcc",
            ":libc_version_gnu",
        ],
        toolchain = ":cc-compiler-gcc-x86_64-gnu",
        toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
    )

def _gcc_x86_64_static_musl():
    tool_paths = {
        "ar": "/usr/bin/ar",
        "cpp": "/usr/bin/cpp",
        "dwp": "/usr/bin/dwp",
        "gcc": "/usr/bin/gcc-11",
        "gcov": "/usr/bin/gcov",
        "ld": "/usr/bin/ld",
        "llvm-cov": "/opt/clang-15.0/bin/llvm-cov",
        "nm": "/usr/bin/nm",
        "objcopy": "/usr/bin/objcopy",
        "objdump": "/usr/bin/objdump",
        "strip": "/usr/bin/strip",
    }

    musl_gcc_lib_path = "/opt/gcc-musl-11.2.0/lib/gcc/x86_64-alpine-linux-musl/11.2.0"
    musl_sysroot_lib_path = "/opt/gcc-musl-11.2.0/x86_64-alpine-linux-musl/lib"

    builtin_includes = [
        "/opt/gcc-musl-11.2.0/x86_64-alpine-linux-musl/include/c++/11.2.0",
        "/opt/gcc-musl-11.2.0/x86_64-alpine-linux-musl/include/c++/11.2.0/x86_64-alpine-linux-musl",
        "/opt/gcc-musl-11.2.0/x86_64-alpine-linux-musl/include/c++/11.2.0/backward",
        "/opt/gcc-musl-11.2.0/x86_64-alpine-linux-musl/include",
        "/opt/gcc-musl-11.2.0/lib/gcc/x86_64-alpine-linux-musl/11.2.0/include",
    ]

    cc_toolchain_config(
        name = "gcc_config_x84_64_static_musl",
        cpu = "k8",
        compiler = "gcc",
        toolchain_identifier = "gcc-x86_64-linux-static-musl",
        host_system_name = "x86_64-unknown-linux-musl",
        target_system_name = "x86_64-unknown-linux-musl",
        target_libc = "musl_unknown",
        abi_version = "gcc",
        abi_libc_version = "musl_unknown",
        cxx_builtin_include_directories = builtin_includes,
        tool_paths = tool_paths,
        compile_flags = [
            "-fstack-protector",
            "-Wall",
            "-Wunused-but-set-parameter",
            "-Wno-free-nonheap-object",
            "-fno-omit-frame-pointer",
            "-nostdinc",
        ] + [
            "-isystem" + path
            for path in builtin_includes
        ],
        cxx_flags = [
            "-std=c++17",
            "-nostdinc++",
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
        link_flags = [
            "-nostdlib",
            "-Wl,-no-as-needed",
            "-Wl,-z,relro,-z,now",
            "-pass-exit-codes",
        ],
        unfiltered_link_flags = [
            "-L" + musl_gcc_lib_path,
            "-L" + musl_sysroot_lib_path,
            "-Wl,-Bstatic,-lm,-lc,-lstdc++,-lm,-lgcc,-lgcc_eh,-Bdynamic",
        ],
        opt_link_flags = ["-Wl,--gc-sections"],
        unfiltered_compile_flags = [
            "-fno-canonical-system-headers",
            "-Wno-builtin-macro-redefined",
            "-D__DATE__=\"redacted\"",
            "-D__TIMESTAMP__=\"redacted\"",
            "-D__TIME__=\"redacted\"",
        ],
        coverage_compile_flags = ["--coverage"],
        coverage_link_flags = ["--coverage"],
        supports_start_end_lib = True,
        # The -nostdlib flag means we have to provide our own c runtime objects.
        custom_c_runtime_paths = {
            "gcc": musl_gcc_lib_path,
            "sysroot": musl_sysroot_lib_path,
        },
    )

    cc_toolchain(
        name = "cc-compiler-gcc-x86_64-static-musl",
        toolchain_identifier = "gcc-x86_64-linux-static-musl",
        toolchain_config = "gcc_config_x84_64_static_musl",
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
        name = "cc-toolchain-gcc-x86_64-static-musl",
        exec_compatible_with = [
            "@platforms//cpu:x86_64",
            "@platforms//os:linux",
        ],
        target_compatible_with = [
            "@platforms//cpu:x86_64",
            "@platforms//os:linux",
        ],
        target_settings = [
            ":compiler_gcc",
            ":libc_version_static_musl",
        ],
        toolchain = ":cc-compiler-gcc-x86_64-static-musl",
        toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
    )

gcc_x86_64_gnu = _gcc_x86_64_gnu
gcc_x86_64_static_musl = _gcc_x86_64_static_musl
