load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "musl",
    lib_source = ":all",
    configure_options = ["--disable-shared"],
    out_static_libs = ["libc.a"],
    visibility = ["//visibility:public"],
)
