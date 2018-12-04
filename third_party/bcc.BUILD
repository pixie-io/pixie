licenses(["notice"])

cc_library(
    name = "bcc",
    hdrs = glob(["include/**/*.h"]),
    srcs = [
      "lib/libbcc.a",
      "lib/libbpf.a",
      "lib/libapi-static.a",
      "lib/libb_frontend.a",
      "lib/libclang_frontend.a",
      "lib/libusdt-static.a",
      "lib/libbcc-loader-static.a",
    ],
    deps = [
      "@com_llvm_lib//:llvm",
    ],
    includes = [
        # TODO(zasgar): Ideally this should be in a directory called bcc or something.
        "include",
    ],
    visibility = ["//visibility:public"],
    linkopts = ["-lz", "-lrt", "-ldl", "-lelf", "-lpthread", "-ltinfo"],
)
    
