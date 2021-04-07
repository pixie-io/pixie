load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])
# MIT LICENSE

exports_files(["license.txt"])

cc_library(
    name = "rapidjson",
    srcs = [],
    hdrs = glob(["include/**/*.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
)
