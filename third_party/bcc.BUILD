licenses(["notice"])

cc_library(
    name = "bcc",
    # do not sort.
    srcs = [
        "lib/libbcc.a",
        "lib/libbpf.a",
        "lib/libapi-static.a",
        "lib/libb_frontend.a",
        "lib/libclang_frontend.a",
        "lib/libusdt-static.a",
        "lib/libbcc-loader-static.a",
    ],
    hdrs = glob(["include/**/*.h"]),
    includes = [
        # TODO(zasgar): Ideally this should be in a directory called bcc or something.
        "include",
        "include/bcc",
    ],
    linkopts = [
        "-lz",
        "-lrt",
        "-ldl",
        "-lelf",
        "-lpthread",
        "-ltinfo",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_llvm_lib//:llvm",
    ],
)
