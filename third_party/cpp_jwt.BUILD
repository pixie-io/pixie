licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "cpp_jwt",
    hdrs = glob(["include/jwt/**/*.hpp", "include/jwt/**/*.ipp"]),
    includes = ["include"],
    copts = ["-Iinclude"],
    visibility = ["//visibility:public"],
    deps = [
      "@boringssl//:ssl",
    ],
)
