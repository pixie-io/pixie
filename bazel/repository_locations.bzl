REPOSITORY_LOCATIONS = dict(
    bazel_gazelle = dict(
        sha256 = "7949fc6cc17b5b191103e97481cf8889217263acf52e00b560683413af204fcb",
        urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/0.16.0/bazel-gazelle-0.16.0.tar.gz"],
    ),
    io_bazel_rules_go = dict(
        sha256 = "492c3ac68ed9dcf527a07e6a1b2dcbf199c6bf8b35517951467ac32e421c06c1",
        urls = ["https://github.com/bazelbuild/rules_go/releases/download/0.17.0/rules_go-0.17.0.tar.gz"],
    ),
    com_github_bazelbuild_buildtools = dict(
        sha256 = "9da0e2911d78554be7d926d6e7a360d62856951da052108ee0772258b2b5c800",
        strip_prefix = "buildtools-db073457c5a56d810e46efc18bb93a4fd7aa7b5e",
        urls = ["https://github.com/bazelbuild/buildtools/archive/db073457c5a56d810e46efc18bb93a4fd7aa7b5e.tar.gz"],
    ),
    com_google_benchmark = dict(
        sha256 = "b3cded2d66d5ea14a135701784959de62042a484b6d5a45da3c9a1d9597b8c7b",
        strip_prefix = "benchmark-eafa34a5e80c352b078307be312d3fafd0a5d13e",
        urls = ["https://github.com/google/benchmark/archive/eafa34a5e80c352b078307be312d3fafd0a5d13e.tar.gz"],
    ),
    io_bazel_rules_docker = dict(
        sha256 = "29d109605e0d6f9c892584f07275b8c9260803bf0c6fcb7de2623b2bedc910bd",
        strip_prefix = "rules_docker-0.5.1",
        urls = ["https://github.com/bazelbuild/rules_docker/archive/v0.5.1.tar.gz"],
    ),
    com_google_googletest = dict(
        sha256 = "9bf1fe5182a604b4135edc1a425ae356c9ad15e9b23f9f12a02e80184c3a249c",
        strip_prefix = "googletest-release-1.8.1",
        urls = ["https://github.com/google/googletest/archive/release-1.8.1.tar.gz"],
    ),
    com_github_grpc_grpc = dict(
        sha256 = "069a52a166382dd7b99bf8e7e805f6af40d797cfcee5f80e530ca3fc75fd06e2",
        strip_prefix = "grpc-1.18.0",
        urls = ["https://github.com/grpc/grpc/archive/v1.18.0.tar.gz"],
    ),
    com_github_gflags_gflags = dict(
        sha256 = "9e1a38e2dcbb20bb10891b5a171de2e5da70e0a50fff34dd4b0c2c6d75043909",
        strip_prefix = "gflags-524b83d0264cb9f1b2d134c564ef1aa23f207a41",
        urls = ["https://github.com/gflags/gflags/archive/524b83d0264cb9f1b2d134c564ef1aa23f207a41.tar.gz"],
    ),
    com_github_google_glog = dict(
        sha256 = "eaabbfc16ecfacb36960ca9c8977f40172c51e4b03234331a1f84040a77ab12c",
        strip_prefix = "glog-781096619d3dd368cfebd33889e417a168493ce7",
        urls = ["https://github.com/google/glog/archive/781096619d3dd368cfebd33889e417a168493ce7.tar.gz"],
    ),
    com_google_absl = dict(
        sha256 = "e35082e88b9da04f4d68094c05ba112502a5063712f3021adfa465306d238c76",
        strip_prefix = "abseil-cpp-cc8dcd307b76a575d2e3e0958a4fe4c7193c2f68",
        urls = ["https://github.com/abseil/abseil-cpp/archive/cc8dcd307b76a575d2e3e0958a4fe4c7193c2f68.tar.gz"],
    ),
    com_google_flatbuffers = dict(
        sha256 = "b2bb0311ca40b12ebe36671bdda350b10c7728caf0cfe2d432ea3b6e409016f3",
        strip_prefix = "flatbuffers-1f5eae5d6a135ff6811724f6c57f911d1f46bb15",
        urls = ["https://github.com/google/flatbuffers/archive/1f5eae5d6a135ff6811724f6c57f911d1f46bb15.tar.gz"],
    ),
    com_google_double_conversion = dict(
        sha256 = "2d589cbdcde9c8e611ecfb8cc570715a618d3c2503fa983f87ac88afac68d1bf",
        strip_prefix = "double-conversion-4199ef3d456ed0549e5665cf4186f0ee6210db3b",
        urls = ["https://github.com/google/double-conversion/archive/4199ef3d456ed0549e5665cf4186f0ee6210db3b.tar.gz"],
    ),
    com_intel_tbb = dict(
        sha256 = "5a05cf61d773edbed326a4635d31e84876c46f6f9ed00c9ee709f126904030d6",
        strip_prefix = "tbb-8ff3697f544c5a8728146b70ae3a978025be1f3e",
        urls = ["https://github.com/01org/tbb/archive/8ff3697f544c5a8728146b70ae3a978025be1f3e.tar.gz"],
    ),
    com_efficient_libcuckoo = dict(
        sha256 = "a159d52272d7f60d15d60da2887e764b92c32554750a3ba7ff75c1be8bacd61b",
        strip_prefix = "libcuckoo-f3138045810b2c2e9b59dbede296b4a5194af4f9",
        urls = ["https://github.com/efficient/libcuckoo/archive/f3138045810b2c2e9b59dbede296b4a5194af4f9.zip"],
    ),
)
