# Based on envoy(28d5f41) envoy/bazel/envoy_build_system.bzl
# Compute the final copts based on various options.
def pl_copts(repository, test = False):
    posix_options = [
        "-Wall",
        "-Wno-old-style-cast",
        "-Wextra",
        "-Wnon-virtual-dtor",
        "-Woverloaded-virtual",
        "-Wold-style-cast",
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
def pl_cc_library(
        name,
        srcs = [],
        hdrs = [],
        copts = [],
        visibility = None,
        external_deps = [],
        repository = "",
        linkstamp = None,
        tags = [],
        deps = [],
        strip_include_prefix = None):
    native.cc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        copts = pl_copts(repository) + copts,
        visibility = visibility,
        tags = tags,
        deps = deps + _default_external_deps(),
        alwayslink = 1,
        linkstatic = 1,
        linkstamp = linkstamp,
        strip_include_prefix = strip_include_prefix,
    )

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
            repository + "//test:main",
        ] + _default_external_deps(),
        # from https://github.com/google/googletest/blob/
        #6e1970e2376c14bf658eb88f655a054030353f9f/googlemock/src/gmock.cc#L51
        # 2 - by default, mocks act as StrictMocks.
        args = args + ["--gmock_default_mock_behavior=2"],
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
        alwayslink = 1,
        linkstatic = 1,
    )

# PL C++ mock targets should be specified with this function.
def pl_cc_mock(name, **kargs):
    pl_cc_test_library(name = name, **kargs)
