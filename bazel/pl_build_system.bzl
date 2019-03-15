# Based on envoy(28d5f41) envoy/bazel/envoy_build_system.bzl
# Compute the final copts based on various options.
def pl_copts(repository, test = False):
    posix_options = [
        # Warnings setup.
        "-Wall",
        "-Werror",
        "-Wextra",
        "-Wnon-virtual-dtor",
        "-Woverloaded-virtual",
        "-Wold-style-cast",
        "-Wimplicit-fallthrough",
        "-Wfloat-conversion",
        # Language flags.
        "-std=c++17",
    ]

    return posix_options

def pl_static_link_libstdcpp_linkopts():
    return select({
        "@bazel_tools//tools/osx:darwin": [],
        "//conditions:default": ["-static-libstdc++", "-static-libgcc"],
    })

# Compute the final linkopts based on various options.
def pl_linkopts():
    return select({
        # The OSX system library transitively links common libraries (e.g., pthread).
        "@bazel_tools//tools/osx:darwin": [],
        "//conditions:default": [
            "-pthread",
            "-lrt",
            "-ldl",
            "-Wl,--hash-style=gnu",
        ],
    }) + pl_static_link_libstdcpp_linkopts()

# Compute the test linkopts.
def pl_test_linkopts():
    return select({
        "@bazel_tools//tools/osx:darwin": [],
        "//conditions:default": [
            "-pthread",
            "-lrt",
            "-ldl",
        ],
    })

def _default_external_deps():
    return [
        "@com_github_gflags_gflags//:gflags",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:base",
        "@com_google_absl//absl/strings:strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/debugging:symbolize",
        "@com_google_absl//absl/debugging:failure_signal_handler",
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
        repository = "",
        linkstamp = None,
        linkopts = [],
        tags = [],
        testonly = 0,
        deps = [],
        strip_include_prefix = None):
    native.cc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        copts = pl_copts(repository) + copts,
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
    if "deps" in kwargs:
        kwargs["deps"] = kwargs["deps"] + ["//src/common:common"]
    else:
        kwargs["deps"] = ["//src/common:common"]
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
        linkopts = []):
    if not linkopts:
        linkopts = pl_linkopts()
    deps = deps
    native.cc_binary(
        name = name,
        srcs = srcs,
        data = data,
        copts = pl_copts(repository),
        linkopts = linkopts,
        testonly = testonly,
        linkstatic = 1,
        visibility = visibility,
        stamp = 1,
        tags = tags,
        deps = deps + _default_external_deps(),
    )

# PL C++ test targets should be specified with this function.
def pl_cc_test(
        name,
        srcs = [],
        data = [],
        repository = "",
        deps = [],
        tags = [],
        args = [],
        coverage = True,
        local = False):
    test_lib_tags = []
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
        copts = pl_copts(repository, test = True),
        linkopts = pl_test_linkopts(),
        linkstatic = 1,
        deps = [
            ":" + name + "_lib",
            repository + "//src/test_utils:main",
        ] + _default_external_deps(),
        args = args,
        tags = tags + ["coverage_test"],
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
        copts = pl_copts(repository, test = True),
        testonly = 1,
        deps = deps + [
            "@com_google_googletest//:gtest",
        ] + _default_external_deps(),
        tags = tags,
        visibility = visibility,
        alwayslink = 1,
        linkstatic = 1,
    )

# PL C++ mock targets should be specified with this function.
def pl_cc_mock(name, **kargs):
    pl_cc_test_library(name = name, **kargs)
