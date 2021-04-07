load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # MIT

cc_library(
    name = "picohttpparser",
    srcs = ["picohttpparser.c"],
    hdrs = glob(["*"]),
    includes = ["."],
    visibility = ["//visibility:public"],
)
