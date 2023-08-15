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

# Based on envoy(28d5f41) envoy/bazel/envoy_build_system.bzl
# Compute the final copts based on various options.

load("@io_bazel_rules_docker//go:image.bzl", "go_image")
load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_context", "go_library", "go_test")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("@rules_python//python:defs.bzl", "py_test")
load("//bazel:toolchain_transitions.bzl", "qemu_interactive_runner")

pl_boringcrypto_go_sdk = ["1.20.4"]
pl_supported_go_sdk_versions = ["1.16", "1.17", "1.18", "1.19", "1.20", "1.21"]

# The last version in this list corresponds to the boringcrypto go sdk version.
pl_all_supported_go_sdk_versions = pl_supported_go_sdk_versions + pl_boringcrypto_go_sdk

def pl_go_sdk_version_template_to_label(tpl, version):
    # If version matches sdk configured to use boringcrypto
    # the label name should not contain the sdk version string
    if version in pl_boringcrypto_go_sdk:
        return tpl % "boringcrypto"
    return tpl % version.replace(".", "_")

def pl_copts():
    posix_options = [
        # Warnings setup.
        "-Wall",
        "-Werror",
        "-Wextra",
        "-Wimplicit-fallthrough",
        "-Wfloat-conversion",
        "-Wno-deprecated-declarations",
    ]

    # Since abseil's BUILD.bazel doesn't provide any system 'includes', add them in manually here.
    # In contrast, libraries like googletest do provide includes, so no need to add those.
    manual_system_includes = ["-isystem external/com_google_absl", "-isystem external/org_tensorflow"]

    tcmalloc_flags = select({
        "@px//bazel:disable_tcmalloc": ["-DABSL_MALLOC_HOOK_MMAP_DISABLE=1"],
        "//conditions:default": ["-DTCMALLOC=1"],
    }) + select({
        "@px//bazel:debug_tcmalloc": ["-DPL_MEMORY_DEBUG_ENABLED=1"],
        "//conditions:default": [],
    })

    # Leaving this here as an example of how to add compiler dependent_flags.
    compiler_dependent_flags = select({
        "@px//bazel:gcc_build": [
            # Since we globally disable these warnings in the .bazelrc file,
            # we force them enabled for our own source code.
            "-Werror=stringop-truncation",
            "-Werror=maybe-uninitialized",
        ],
        "//conditions:default": [
            # TODO(yzhao/oazizi): Please remove this after fixing warnings in stirling.
            "-Wno-c99-designator",
        ],
    })

    return posix_options + manual_system_includes + tcmalloc_flags + compiler_dependent_flags

# Compute the final linkopts based on various options.
def pl_linkopts():
    return pl_common_linkopts()

# Compute the test linkopts.
def pl_test_linkopts():
    return pl_common_linkopts()

def pl_common_linkopts():
    return select({
        "//bazel:gcc_build": [
            "-pthread",
            "-llzma",
            "-lrt",
            "-ldl",
            "-Wl,--hash-style=gnu",
            "-lunwind",
        ],
        # The OSX system library transitively links common libraries (e.g., pthread).
        "@bazel_tools//tools/osx:darwin": [],
        "//conditions:default": [
            "-pthread",
            "-lunwind",
            "-llzma",
            "-lrt",
            "-ldl",
            "-Wl,--hash-style=gnu",
        ],
    }) + select({
        "//bazel:use_libcpp": [],
        "//conditions:default": ["-lstdc++fs"],
    })

def pl_defines():
    return ["MAGIC_ENUM_RANGE_MIN=-128", "MAGIC_ENUM_RANGE_MAX=256"]

def _default_external_deps():
    return [
        "@com_github_gflags_gflags//:gflags",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:base",
        "@com_google_absl//absl/strings:strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/debugging:symbolize",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_absl//absl/functional:bind_front",
        "@com_github_neargye_magic_enum//:magic_enum",
    ]

def _default_internal_deps():
    return [
        "//src/common/base:cc_library",
        "//src/common/memory:cc_library",
        "//src/common/perf:cc_library",
    ]

def pl_default_features():
    return [
        "-external_dep",
    ]

# PL C++ library targets should be specified with this function.
def pl_cc_library_internal(
        name,
        srcs = [],
        hdrs = [],
        data = [],
        copts = [],
        includes = [],
        visibility = None,
        tcmalloc_dep = False,
        repository = "",
        linkstamp = None,
        linkopts = [],
        local_defines = [],
        defines = [],
        tags = [],
        testonly = 0,
        deps = [],
        strip_include_prefix = None):
    if tcmalloc_dep:
        deps += tcmalloc_external_deps(repository)
    cc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        data = data,
        copts = pl_copts() + copts,
        includes = includes,
        visibility = visibility,
        tags = tags,
        deps = deps + _default_external_deps(),
        alwayslink = 1,
        linkstatic = 1,
        linkstamp = linkstamp,
        linkopts = linkopts,
        local_defines = local_defines,
        defines = pl_defines() + defines,
        testonly = testonly,
        strip_include_prefix = strip_include_prefix,
        features = pl_default_features(),
    )

def pl_cc_library(**kwargs):
    kwargs["deps"] = kwargs.get("deps", [])
    kwargs["deps"] = kwargs["deps"] + _default_internal_deps()

    pl_cc_library_internal(**kwargs)

# PL C++ binary targets should be specified with this function.
def pl_cc_binary(
        name,
        srcs = [],
        data = [],
        args = [],
        testonly = 0,
        visibility = None,
        repository = "",
        stamp = 0,
        tags = [],
        deps = [],
        copts = [],
        linkopts = [],
        defines = []):
    if not linkopts:
        linkopts = pl_linkopts()
    deps = deps
    cc_binary(
        name = name,
        srcs = srcs,
        data = data,
        args = args,
        copts = pl_copts() + copts,
        linkopts = linkopts,
        testonly = testonly,
        linkstatic = 1,
        visibility = visibility,
        malloc = tcmalloc_external_dep(repository),
        stamp = stamp,
        tags = tags,
        deps = deps + _default_external_deps() + _default_internal_deps(),
        defines = pl_defines() + defines,
        features = pl_default_features(),
    )

def _default_empty_list(name, kwargs):
    if not name in kwargs:
        kwargs[name] = []

def pl_cc_musl_binary(name, **kwargs):
    _default_empty_list("copts", kwargs)
    kwargs["copts"] = kwargs["copts"] + ["-nostdlib", "-nostdinc"]

    _default_empty_list("additional_linker_inputs", kwargs)
    kwargs["additional_linker_inputs"] = kwargs["additional_linker_inputs"] + [
        "@org_libc_musl//:crt1.o",
        "@org_libc_musl//:crti.o",
        "@org_libc_musl//:crtn.o",
    ]

    linkshared = "linkshared" in kwargs and kwargs["linkshared"]
    start_libs = []
    if not linkshared:
        start_libs = start_libs + ["$(location @org_libc_musl//:crt1.o)"]
    start_libs = start_libs + ["$(location @org_libc_musl//:crti.o)"]
    end_libs = ["$(location @org_libc_musl//:crtn.o)"]
    linkopts = [
        "-nostdlib",
        "-nodefaultlibs",
        "-nostartfiles",
        "-lgcc",
    ]
    _default_empty_list("linkopts", kwargs)
    kwargs["linkopts"] = start_libs + kwargs["linkopts"] + linkopts + end_libs

    _default_empty_list("deps", kwargs)
    kwargs["deps"] = kwargs["deps"] + ["@org_libc_musl//:musl"]
    cc_binary(name = name, **kwargs)

# PL C++ test targets should be specified with this function.
def pl_cc_test(
        name,
        srcs = [],
        data = [],
        repository = "",
        deps = [],
        tags = [],
        shard_count = None,
        size = "small",
        timeout = "short",
        args = [],
        defines = [],
        coverage = True,
        local = False,
        flaky = False,
        **kwargs):
    test_lib_tags = list(tags)
    if coverage:
        test_lib_tags.append("coverage_test_lib")
    pl_cc_test_library(
        name = name + "_lib",
        srcs = srcs,
        data = data,
        deps = deps,
        repository = repository,
        tags = test_lib_tags,
        defines = defines,
    )
    cc_test(
        name = name,
        copts = pl_copts(),
        linkopts = pl_test_linkopts(),
        linkstatic = 1,
        malloc = tcmalloc_external_dep(repository),
        deps = [
            ":" + name + "_lib",
            repository + "//src/common/testing:test_main",
            repository + "//src/shared/version:test_version_linkstamp",
        ] + _default_external_deps(),
        args = args,
        data = data + ["//bazel/test_runners:test_runner_dep"],
        tags = tags + ["coverage_test"],
        shard_count = shard_count,
        size = size,
        timeout = timeout,
        local = local,
        flaky = flaky,
        features = pl_default_features(),
        **kwargs
    )

# PL C++ test related libraries (that want gtest, gmock) should be specified
# with this function.
def pl_cc_test_library(
        name,
        srcs = [],
        hdrs = [],
        data = [],
        deps = [],
        visibility = None,
        repository = "",
        tags = [],
        defines = []):
    cc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        data = data,
        copts = pl_copts(),
        testonly = 1,
        deps = deps + [
            "@com_google_googletest//:gtest",
            repository + "//src/common/testing:cc_library",
        ] + _default_external_deps(),
        tags = tags,
        defines = pl_defines() + defines,
        visibility = visibility,
        alwayslink = 1,
        linkstatic = 1,
        features = pl_default_features(),
    )

# PL C++ mock targets should be specified with this function.
def pl_cc_mock(name, **kargs):
    pl_cc_test_library(name = name, **kargs)

# Dependencies on tcmalloc_and_profiler should be wrapped with this function.
def tcmalloc_external_dep(repository):
    return select({
        repository + "//bazel:disable_tcmalloc": None,
        "//conditions:default": "//third_party:gperftools",
    })

# As above, but wrapped in list form for adding to dep lists. This smell seems needed as
# SelectorValue values have to match the attribute type. See
# https://github.com/bazelbuild/bazel/issues/2273.
def tcmalloc_external_deps(repository):
    return select({
        repository + "//bazel:disable_tcmalloc": [],
        "//conditions:default": ["//third_party:gperftools"],
    })

def pl_cgo_library(**kwargs):
    if "cgo" in kwargs and kwargs["cgo"] and "clinkopts" not in kwargs:
        kwargs["clinkopts"] = pl_linkopts() + select({
            "@px//bazel:coverage_enabled": ["-lgcov --coverage"],
            "//conditions:default": [],
        })
        kwargs["toolchains"] = ["@bazel_tools//tools/cpp:current_cc_toolchain"]
    go_library(**kwargs)

def _pl_bindata_impl(ctx):
    """
    Copied from https://github.com/bazelbuild/rules_go/blob/master/extras/bindata.bzl
    but updated to change the bindata executable and to strip the bin_dir path.
    """
    go = go_context(ctx)
    out = go.declare_file(go, ext = ".gen.go")
    arguments = ctx.actions.args()
    arguments.add_all([
        "-o",
        out,
        "-pkg",
        ctx.attr.package,
        "-prefix",
        ctx.label.package,
    ])
    if ctx.attr.strip_bin_dir:
        arguments.add_all([
            "-prefix",
            ctx.bin_dir.path,
        ])
    if not ctx.attr.compress:
        arguments.add("-nocompress")
    if not ctx.attr.metadata:
        arguments.add("-nometadata")
    if not ctx.attr.memcopy:
        arguments.add("-nomemcopy")
    if not ctx.attr.modtime:
        arguments.add_all(["-modtime", "0"])
    if ctx.attr.extra_args:
        arguments.add_all(ctx.attr.extra_args)
    srcs = [f.path for f in ctx.files.srcs]
    if ctx.attr.strip_external and any([f.startswith("external/") for f in srcs]):
        arguments.add("-prefix", ctx.label.workspace_root + "/" + ctx.label.package)
    arguments.add_all(srcs)
    ctx.actions.run(
        inputs = ctx.files.srcs,
        outputs = [out],
        mnemonic = "GoBindata",
        executable = ctx.executable._bindata,
        arguments = [arguments],
    )
    return [
        DefaultInfo(
            files = depset([out]),
        ),
    ]

pl_bindata = rule(
    _pl_bindata_impl,
    toolchains = ["@io_bazel_rules_go//go:toolchain"],
    attrs = {
        "compress": attr.bool(default = True),
        "extra_args": attr.string_list(),
        "memcopy": attr.bool(default = True),
        "metadata": attr.bool(default = False),
        "modtime": attr.bool(default = False),
        "package": attr.string(mandatory = True),
        "srcs": attr.label_list(allow_files = True),
        # Modification of the original bindata arguments.
        "strip_bin_dir": attr.bool(default = False),
        "strip_external": attr.bool(default = False),
        "_bindata": attr.label(
            executable = True,
            cfg = "exec",
            # Modification of go_bindata repo from kevinburke to the go-bindata repo.
            default = "@com_github_go_bindata_go_bindata//go-bindata:go-bindata",
        ),
        "_go_context_data": attr.label(default = "@io_bazel_rules_go//:go_context_data"),
    },
)

def pl_go_image(**kwargs):
    base = "//:pl_go_base_image"
    if "base" not in kwargs:
        kwargs["base"] = base
    go_image(
        **kwargs
    )

def _add_no_pie(kwargs):
    if "gc_linkopts" not in kwargs:
        kwargs["gc_linkopts"] = []
    kwargs["gc_linkopts"].append("-extldflags")
    kwargs["gc_linkopts"].append("-no-pie")

def _add_test_runner(kwargs):
    if "data" not in kwargs:
        kwargs["data"] = []
    kwargs["data"].append("//bazel/test_runners:test_runner_dep")

def _add_no_sysroot(kwargs):
    if "target_compatible_with" not in kwargs:
        kwargs["target_compatible_with"] = []
    kwargs["target_compatible_with"] = kwargs["target_compatible_with"] + select({
        "//bazel/cc_toolchains:libc_version_glibc_host": [],
        "//conditions:default": ["@platforms//:incompatible"],
    })

def pl_go_test(**kwargs):
    _add_no_pie(kwargs)
    _add_test_runner(kwargs)
    go_test(**kwargs)

def pl_go_binary(**kwargs):
    _add_no_pie(kwargs)
    go_binary(**kwargs)

def pl_py_test(**kwargs):
    _add_test_runner(kwargs)
    _add_no_sysroot(kwargs)
    py_test(**kwargs)

def pl_sh_test(**kwargs):
    _add_test_runner(kwargs)
    native.sh_test(**kwargs)

def pl_cc_bpf_test(**kwargs):
    pl_cc_test(**kwargs)
    qemu_interactive_runner(
        name = kwargs["name"] + "_qemu_interactive",
        test = ":" + kwargs["name"],
        tags = [
            "manual",
            "qemu_interactive",
        ],
        testonly = True,
    )

def pl_sh_bpf_test(**kwargs):
    pl_sh_test(**kwargs)
    qemu_interactive_runner(
        name = kwargs["name"] + "_qemu_interactive",
        test = ":" + kwargs["name"],
        tags = [
            "manual",
            "qemu_interactive",
        ],
        testonly = True,
    )
