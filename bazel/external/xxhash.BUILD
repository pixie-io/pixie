load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "xxhash",
    hdrs = glob([
        "xxhash.h",
        "xxh3.h",
    ]),
    defines = ["XXH_INLINE_ALL"],
    visibility = ["//visibility:public"],
)
