REPOSITORY_LOCATIONS = dict(
    bazel_gazelle = dict(
        sha256 = "c0a5739d12c6d05b6c1ad56f2200cb0b57c5a70e03ebd2f7b87ce88cabf09c7b",
        urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/0.14.0/bazel-gazelle-0.14.0.tar.gz"],
    ),
    io_bazel_rules_go = dict(
        sha256 = "97cf62bdef33519412167fd1e4b0810a318a7c234f5f8dc4f53e2da86241c492",
        urls = ["https://github.com/bazelbuild/rules_go/releases/download/0.15.3/rules_go-0.15.3.tar.gz"],
    ),
    com_github_bazelbuild_buildtools = dict(
        sha256 = "82ce8e14772b42cfdcfa46075e6fb7e1c7363b8fb8dadc74caa30cff56d6646f",
        strip_prefix = "buildtools-4a7914a1466ff7388c934bfcd43a3852928536f6",
        urls = ["https://github.com/bazelbuild/buildtools/archive/4a7914a1466ff7388c934bfcd43a3852928536f6.tar.gz"],
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
        sha256 = "2925dcdea4595a62e439355823984df061e085485d7c2208802ed944ded62b5b",
        strip_prefix = "grpc-d8020cb6daa87f1a3bb3b0c299bc081c4a3de1e8",
        urls = ["https://github.com/grpc/grpc/archive/d8020cb6daa87f1a3bb3b0c299bc081c4a3de1e8.tar.gz"],
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
)
