workspace(name = "pl")

load("//:workspace.bzl", "check_min_bazel_version")
check_min_bazel_version("0.15.0")


##########################################################
# Bazel Go setup.
##########################################################
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "io_bazel_rules_go",
    urls = ["https://github.com/bazelbuild/rules_go/releases/download/0.14.0/rules_go-0.14.0.tar.gz"],
    sha256 = "5756a4ad75b3703eb68249d50e23f5d64eaf1593e886b9aa931aa6e938c4e301",
)
http_archive(
    name = "bazel_gazelle",
    urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/0.14.0/bazel-gazelle-0.14.0.tar.gz"],
    sha256 = "c0a5739d12c6d05b6c1ad56f2200cb0b57c5a70e03ebd2f7b87ce88cabf09c7b",
)
load("@io_bazel_rules_go//go:def.bzl", "go_rules_dependencies", "go_register_toolchains")
go_rules_dependencies()
go_register_toolchains()
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
gazelle_dependencies()


##########################################################
# Bazel CC setup.
##########################################################
# Google test related rules.
new_http_archive(
    name = "googletest",
    build_file = "third_party/gtest.BUILD",
    strip_prefix = "googletest-release-1.8.0",
    url = "https://github.com/google/googletest/archive/release-1.8.0.zip",
)

bind(
    name = "gtest",
    actual = "@googletest//:gtest",
)

bind(
    name = "gtest-main",
    actual = "@googletest//:gtest-main",
)
