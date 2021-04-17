load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # MIT

exports_files(["COPYING"])

cc_library(
    name = "farmhash",
    srcs = ["src/farmhash.cc"],
    hdrs = ["src/farmhash.h"],
    includes = ["src"],
    visibility = ["//visibility:public"],
)
