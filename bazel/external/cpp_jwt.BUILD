load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "cpp_jwt",
    hdrs = glob([
        "include/jwt/**/*.hpp",
        "include/jwt/**/*.ipp",
    ]),
    copts = ["-Iinclude"],
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        "@boringssl//:ssl",
    ],
)
