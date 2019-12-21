# Based on envoy(28d5f41) envoy/bazel/envoy_build_system.bzl
# Compute the final copts based on various options.
load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library", "go_test")

def pl_copts():
    posix_options = [
        # Warnings setup.
        "-Wall",
        "-Werror",
        "-Wextra",
        "-Wimplicit-fallthrough",
        "-Wfloat-conversion",
    ]

    # Since abseil's BUILD.bazel doesn't provide any system 'includes', add them in manually here.
    # In contrast, libraries like googletest do provide includes, so no need to add those.
    manual_system_includes = ["-isystem external/com_google_absl"]

    tcmalloc_flags = select({
        "@pl//bazel:disable_tcmalloc": ["-DABSL_MALLOC_HOOK_MMAP_DISABLE=1"],
        "//conditions:default": ["-DTCMALLOC=1"],
    }) + select({
        "@pl//bazel:debug_tcmalloc": ["-DPL_MEMORY_DEBUG_ENABLED=1"],
        "//conditions:default": [],
    })

    # TODO(zasgar): Remove these flags as we cleanup the code.
    compiler_dependent_flags = select({
        "@pl//bazel:gcc_build": [
            "-Wno-error=deprecated-declarations",
            "-Wno-error=deprecated-copy",
            "-Wno-error=class-memaccess",
        ],
        "//conditions:default": [],
    })

    return posix_options + manual_system_includes + tcmalloc_flags + compiler_dependent_flags

# Compute the final linkopts based on various options.
def pl_linkopts():
    return select({
        # The OSX system library transitively links common libraries (e.g., pthread).
        "@bazel_tools//tools/osx:darwin": [],
        "//conditions:default": [
            "-pthread",
            "-l:libunwind.a",
            "-llzma",
            "-lrt",
            "-ldl",
            "-lstdc++fs",
            "-Wl,--hash-style=gnu",
        ],
    })

# Compute the test linkopts.
def pl_test_linkopts():
    return select({
        "@bazel_tools//tools/osx:darwin": [],
        "//conditions:default": [
            "-pthread",
            "-l:libunwind.a",
            "-llzma",
            "-lrt",
            "-ldl",
            "-lstdc++fs",
        ],
    })

def _default_external_deps():
    return [
        "@com_github_gflags_gflags//:gflags",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:base",
        "@com_google_absl//absl/strings:strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/debugging:symbolize",
        "@com_google_absl//absl/debugging:failure_signal_handler",
    ]

def _default_internal_deps():
    return [
        "//src/common/base:cc_library",
        "//src/common/memory:cc_library",
        "//src/common/perf:cc_library",
    ]

# PL C++ library targets should be specified with this function.
def pl_cc_library_internal(
        name,
        srcs = [],
        hdrs = [],
        copts = [],
        includes = [],
        visibility = None,
        external_deps = [],
        tcmalloc_dep = False,
        repository = "",
        linkstamp = None,
        linkopts = [],
        tags = [],
        testonly = 0,
        deps = [],
        strip_include_prefix = None):
    if tcmalloc_dep:
        deps += tcmalloc_external_deps(repository)

    native.cc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        copts = pl_copts() + copts,
        includes = includes,
        visibility = visibility,
        tags = tags,
        deps = deps + _default_external_deps(),
        alwayslink = 1,
        linkstatic = 1,
        linkstamp = linkstamp,
        linkopts = linkopts,
        testonly = testonly,
        strip_include_prefix = strip_include_prefix,
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
        testonly = 0,
        visibility = None,
        external_deps = [],
        repository = "",
        stamped = False,
        tags = [],
        deps = [],
        copts = [],
        linkopts = []):
    if not linkopts:
        linkopts = pl_linkopts()
    deps = deps
    native.cc_binary(
        name = name,
        srcs = srcs,
        data = data,
        copts = pl_copts() + copts,
        linkopts = linkopts,
        testonly = testonly,
        linkstatic = 1,
        visibility = visibility,
        malloc = tcmalloc_external_dep(repository),
        stamp = 1,
        tags = tags,
        deps = deps + _default_external_deps() + _default_internal_deps(),
    )

# PL C++ test targets should be specified with this function.
def pl_cc_test(
        name,
        srcs = [],
        data = [],
        repository = "",
        deps = [],
        tags = [],
        size = "small",
        timeout = "short",
        args = [],
        coverage = True,
        local = False):
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
    )

    native.cc_test(
        name = name,
        copts = pl_copts(),
        linkopts = pl_test_linkopts(),
        linkstatic = 1,
        malloc = tcmalloc_external_dep(repository),
        deps = [
            ":" + name + "_lib",
            repository + "//src/common/testing:test_main",
        ] + _default_external_deps(),
        args = args,
        data = data,
        tags = tags + ["coverage_test"],
        size = size,
        timeout = timeout,
        local = local,
    )

# PL C++ test related libraries (that want gtest, gmock) should be specified
# with this function.
def pl_cc_test_library(
        name,
        srcs = [],
        hdrs = [],
        data = [],
        external_deps = [],
        deps = [],
        visibility = None,
        repository = "",
        tags = []):
    native.cc_library(
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
        visibility = visibility,
        alwayslink = 1,
        linkstatic = 1,
    )

# PL C++ mock targets should be specified with this function.
def pl_cc_mock(name, **kargs):
    pl_cc_test_library(name = name, **kargs)

# Dependencies on tcmalloc_and_profiler should be wrapped with this function.
def tcmalloc_external_dep(repository):
    return select({
        repository + "//bazel:disable_tcmalloc": None,
        "//conditions:default": "//third_party/foreign_cc:gperftools",
    })

# As above, but wrapped in list form for adding to dep lists. This smell seems needed as
# SelectorValue values have to match the attribute type. See
# https://github.com/bazelbuild/bazel/issues/2273.
def tcmalloc_external_deps(repository):
    return select({
        repository + "//bazel:disable_tcmalloc": [],
        "//conditions:default": ["//third_party/foreign_cc:gperftools"],
    })

def pl_cgo_library(**kwargs):
    if "cgo" in kwargs and kwargs["cgo"] and "clinkopts" not in kwargs:
        kwargs["clinkopts"] = pl_linkopts() + select({
            "@pl//bazel:coverage_enabled": ["-lgcov --coverage"],
            "//conditions:default": [],
        })
        kwargs["toolchains"] = ["@bazel_tools//tools/cpp:current_cc_toolchain"]
    go_library(**kwargs)

def append_manual_tag(kwargs):
    tags = kwargs.get("tags", [])
    kwargs["tags"] = tags + ["manual"]
    return kwargs

def pl_exp_cc_binary(**kwargs):
    kwargs = append_manual_tag(kwargs)
    pl_cc_binary(**kwargs)

def pl_exp_cc_library(**kwargs):
    kwargs = append_manual_tag(kwargs)
    pl_cc_library(**kwargs)

def pl_exp_cc_test(**kwargs):
    kwargs = append_manual_tag(kwargs)
    pl_cc_test(**kwargs)

def pl_exp_cc_test_library(**kwargs):
    kwargs = append_manual_tag(kwargs)
    pl_cc_test_library(**kwargs)
