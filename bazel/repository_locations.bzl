REPOSITORY_LOCATIONS = dict(
    bazel_gazelle = dict(
        sha256 = "cdb02a887a7187ea4d5a27452311a75ed8637379a1287d8eeb952138ea485f7d",
        urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.21.1/bazel-gazelle-v0.21.1.tar.gz"],
    ),
    io_bazel_rules_go = dict(
        # NOTE: Many BPF test programs are written in Go, to avoid accidentally breaking them.
        # Run the following command when upgrading Golang version:
        # scripts/sudo_bazel_run.sh //src/stirling:http2_trace_bpf_test
        #
        # TODO(yzhao): Investigate adding automatic tests when upgrading Golang build toolchain.
        # Today ci/bazel_build_deps.sh does the target search for diffs. It uses bazel query,
        # but cannot look at the content changes. One idea is to have 2 repository_locations.bzl
        # file, one for each language tool chains, and let the language rules declare deps on
        # that .bzl file.
        sha256 = "d1ffd055969c8f8d431e2d439813e42326961d0942bdf734d2c95dc30c369566",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go" +
            "/releases/download/v0.24.5/rules_go-v0.24.5.tar.gz",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.24.5/rules_go-v0.24.5.tar.gz",
        ],
    ),
    io_bazel_rules_k8s = dict(
        sha256 = "cc75cf0d86312e1327d226e980efd3599704e01099b58b3c2fc4efe5e321fcd9",
        strip_prefix = "rules_k8s-0.3.1",
        urls = [
            "https://github.com/bazelbuild/rules_k8s/releases/download/v0.3.1/rules_k8s-v0.3.1.tar.gz",
        ],
    ),
    com_github_bazelbuild_buildtools = dict(
        sha256 = "f3ef44916e6be705ae862c0520bac6834dd2ff1d4ac7e5abc61fe9f12ce7a865",
        strip_prefix = "buildtools-0.29.0",
        urls = ["https://github.com/bazelbuild/buildtools/archive/0.29.0.tar.gz"],
    ),
    com_google_benchmark = dict(
        sha256 = "dccbdab796baa1043f04982147e67bb6e118fe610da2c65f88912d73987e700c",
        strip_prefix = "benchmark-1.5.2",
        urls = ["https://github.com/google/benchmark/archive/v1.5.2.tar.gz"],
    ),
    bazel_skylib = dict(
        sha256 = "e5d90f0ec952883d56747b7604e2a15ee36e288bb556c3d0ed33e818a4d971f2",
        strip_prefix = "bazel-skylib-1.0.2",
        urls = ["https://github.com/bazelbuild/bazel-skylib/archive/1.0.2.tar.gz"],
    ),
    io_bazel_rules_docker = dict(
        sha256 = "1698624e878b0607052ae6131aa216d45ebb63871ec497f26c67455b34119c80",
        strip_prefix = "rules_docker-0.15.0",
        urls = ["https://github.com/bazelbuild/rules_docker/archive/v0.15.0.tar.gz"],
    ),
    io_bazel_toolchains = dict(
        sha256 = "e2126599d29f2028e6b267eba273dcc8e7f4a35ff323e9600cf42fb03875b7c6",
        strip_prefix = "bazel-toolchains-2.0.0",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-toolchains/archive/2.0.0.tar.gz",
            "https://github.com/bazelbuild/bazel-toolchains/archive/2.0.0.tar.gz",
        ],
    ),
    com_google_googletest = dict(
        sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
        strip_prefix = "googletest-release-1.10.0",
        urls = ["https://github.com/google/googletest/archive/release-1.10.0.tar.gz"],
    ),
    # Point to local copy until this is landed and released:
    # https://github.com/grpc/grpc/pull/23561
    com_github_grpc_grpc = dict(
        sha256 = "39d9c14c44bdf97ceb30d1d3af44ab5a2f9982c2329e8a6018c6aa330017f56e",
        strip_prefix = "grpc-f427e35d7a0163fd22be4a72aadb0d67fda0cd57",
        urls = ["https://github.com/pixie-labs/grpc/archive/f427e35d7a0163fd22be4a72aadb0d67fda0cd57.zip"],
        #https://github.com/pixie-labs/grpc/archive/PL_HEAD.zip"],
    ),
    com_google_boringssl = dict(
        sha256 = "781fa39693ec2984c71213cd633e9f6589eaaed75e3a9ac413237edec96fd3b9",
        strip_prefix = "boringssl-83da28a68f32023fd3b95a8ae94991a07b1f6c62",
        urls = ["https://github.com/google/boringssl/" +
                "archive/83da28a68f32023fd3b95a8ae94991a07b1f6c6.tar.gz"],
    ),
    com_github_gflags_gflags = dict(
        sha256 = "9e1a38e2dcbb20bb10891b5a171de2e5da70e0a50fff34dd4b0c2c6d75043909",
        strip_prefix = "gflags-524b83d0264cb9f1b2d134c564ef1aa23f207a41",
        urls = ["https://github.com/gflags/gflags/archive/524b83d0264cb9f1b2d134c564ef1aa23f207a41.tar.gz"],
    ),
    com_github_google_glog = dict(
        sha256 = "51992e76446e384643e474b15d11a3937d2eb9e82c3f546360dfa48e96a0684c",
        # Nov 9, 2019.
        strip_prefix = "glog-1863b4228c85dd88885695476e943a1d5758f8ab",
        urls = ["https://github.com/google/glog/archive/1863b4228c85dd88885695476e943a1d5758f8ab.tar.gz"],
    ),
    com_github_rlyeh_sole = dict(
        sha256 = "0e2d2d280e6847b3301c7302b7924e2841f517985cb189ce0fb94aa9fb5a17c7",
        strip_prefix = "sole-653a25ad03775d7e0a2d50142160795723915ba6",
        urls = ["https://github.com/r-lyeh-archived/sole/archive/653a25ad03775d7e0a2d50142160795723915ba6.tar.gz"],
    ),
    com_google_absl = dict(
        sha256 = "84c9f1312d36546e07b8135143319f0f63f577ca4719286e9e0a0986bfc1b898",
        strip_prefix = "abseil-cpp-c51510d1d87ebce8615ae1752fd5aca912f6cf4c",
        urls = ["https://github.com/abseil/abseil-cpp/archive/c51510d1d87ebce8615ae1752fd5aca912f6cf4c.tar.gz"],
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
        sha256 = "ebc4f6aa47972daed1f7bf71d100ae5bf6931c2e3144cf299c8cc7d041dca2f3",
        strip_prefix = "oneTBB-2020.3",
        urls = ["https://github.com/oneapi-src/oneTBB/archive/v2020.3.tar.gz"],
    ),
    com_github_libarchive_libarchive = dict(
        sha256 = "b60d58d12632ecf1e8fad7316dc82c6b9738a35625746b47ecdcaf4aed176176",
        strip_prefix = "libarchive-3.4.2",
        urls = ["https://github.com/libarchive/libarchive/releases/download/v3.4.2/libarchive-3.4.2.tar.gz"],
    ),
    com_efficient_libcuckoo = dict(
        sha256 = "a159d52272d7f60d15d60da2887e764b92c32554750a3ba7ff75c1be8bacd61b",
        strip_prefix = "libcuckoo-f3138045810b2c2e9b59dbede296b4a5194af4f9",
        urls = ["https://github.com/efficient/libcuckoo/archive/f3138045810b2c2e9b59dbede296b4a5194af4f9.zip"],
    ),
    com_google_farmhash = dict(
        sha256 = "09b5da9eaa7c7f4f073053c1c6c398e320ca917e74e8f366fd84679111e87216",
        strip_prefix = "farmhash-2f0e005b81e296fa6963e395626137cf729b710c",
        urls = ["https://github.com/google/farmhash/archive/2f0e005b81e296fa6963e395626137cf729b710c.tar.gz"],
    ),
    com_github_cpp_taskflow = dict(
        sha256 = "6aee0c20156380d762dd9774ba6bc7d30647d4bec03def2bba4fefef966c3e45",
        strip_prefix = "cpp-taskflow-3c996b520500e0694a26fca743046c54d8ac26cc",
        urls = ["https://github.com/cpp-taskflow/cpp-taskflow/archive/3c996b520500e0694a26fca743046c54d8ac26cc.tar.gz"],
    ),
    com_github_tencent_rapidjson = dict(
        sha256 = "4a34a0c21794f067afca6c9809971f0bd77a1d1834c4dc53bdd09e4ab4d05ce4",
        strip_prefix = "rapidjson-f56928de85d56add3ca6ae7cf7f119a42ee1585b",
        urls = ["https://github.com/Tencent/rapidjson/archive/f56928de85d56add3ca6ae7cf7f119a42ee1585b.tar.gz"],
    ),
    com_github_ariafallah_csv_parser = dict(
        sha256 = "c722047128c97b7a3f38d0c320888d905692945e4a96b6ebd6d208686764644a",
        strip_prefix = "csv-parser-e3c1207f4de50603a4946dc5daa0633ce31a9257",
        urls = ["https://github.com/AriaFallah/csv-parser/archive/e3c1207f4de50603a4946dc5daa0633ce31a9257.tar.gz"],
    ),
    rules_foreign_cc = dict(
        sha256 = "3411b754d3f7a91356c27a93b6a250a6fa22999858b6d118e928c57e1888acf9",
        strip_prefix = "rules_foreign_cc-e6ca4d2cd12be03ccda117b26d59a03de7481dc3",
        # 2019-11-01
        urls = ["https://github.com/bazelbuild/rules_foreign_cc/" +
                "archive/e6ca4d2cd12be03ccda117b26d59a03de7481dc3.tar.gz"],
    ),
    com_github_gperftools_gperftools = dict(
        sha256 = "18574813a062eee487bc1b761e8024a346075a7cb93da19607af362dc09565ef",
        strip_prefix = "gperftools-fc00474ddc21fff618fc3f009b46590e241e425e",
        urls = ["https://github.com/gperftools/gperftools/archive/fc00474ddc21fff618fc3f009b46590e241e425e.tar.gz"],
    ),
    com_github_h2o_picohttpparser = dict(
        sha256 = "cb47971984d77dc81ed5684d51d668a7bc7804d3b7814a3072c2187dfa37a013",
        strip_prefix = "picohttpparser-1d2b8a184e7ebe6651c30dcede37ba1d89691351",
        urls = ["https://github.com/h2o/picohttpparser/archive/1d2b8a184e7ebe6651c30dcede37ba1d89691351.tar.gz"],
    ),
    distroless = dict(
        sha256 = "54273175a54eedc558b8188ca810b184b0784815d3af17cc5fd9c296be4c150e",
        strip_prefix = "distroless-18b2d2c5ebfa58fe3e0e4ee3ffe0e2651ec0f7f6",
        urls = ["https://github.com/GoogleContainerTools/distroless/" +
                "archive/18b2d2c5ebfa58fe3e0e4ee3ffe0e2651ec0f7f6.tar.gz"],
    ),
    com_github_nats_io_natsc = dict(
        sha256 = "14e50dd3cf30c9839aedf7c8929f3d433c0a69af38f13f7baf5491d9ed2ac43b",
        strip_prefix = "nats.c-3b4668698b8510b8f08413a94523b05a8036d9ab",
        urls = ["https://github.com/nats-io/nats.c/archive/3b4668698b8510b8f08413a94523b05a8036d9ab.tar.gz"],
    ),
    com_github_libuv_libuv = dict(
        sha256 = "63794499bf5da1720e249d3fc14ff396b70b8736effe6ce5b4e47e0f3d476467",
        strip_prefix = "libuv-1.33.1",
        urls = ["https://github.com/libuv/libuv/archive/v1.33.1.tar.gz"],
    ),
    com_github_cameron314_concurrentqueue = dict(
        sha256 = "dde227e8fd561b46bdb3c211fa843adc543227b30607acf8eff049006cdffcd1",
        strip_prefix = "concurrentqueue-dea078cf5b6e742cd67a0d725e36f872feca4de4",
        urls = ["https://github.com/cameron314/concurrentqueue/" +
                "archive/dea078cf5b6e742cd67a0d725e36f872feca4de4.tar.gz"],
    ),
    # June 14, 2019.
    com_github_nghttp2_nghttp2 = dict(
        sha256 = "863e366c530d09d7cebce67c6d7449bdb85bccb5ae0ecff84295a80697a6c989",
        strip_prefix = "nghttp2-ee4431344511886efc66395a38b9bf5dddd7151b",
        urls = ["https://github.com/nghttp2/nghttp2/archive/ee4431344511886efc66395a38b9bf5dddd7151b.tar.gz"],
    ),
    com_github_skystrife_cpptoml = dict(
        sha256 = "7bd5ce29434cc24ed8fc87093f031124cd33a84858b309855a0b5dc3f72e0bd5",
        strip_prefix = "cpptoml-fededad7169e538ca47e11a9ee9251bc361a9a65",
        urls = ["https://github.com/skystrife/cpptoml/archive/fededad7169e538ca47e11a9ee9251bc361a9a65.tar.gz"],
    ),
    com_github_neargye_magic_enum = dict(
        sha256 = "4fe6627407a656d0d73879c0346b251ccdcfb718c37bef5410ba172c7c7d5f9a",
        strip_prefix = "magic_enum-0.7.0",
        urls = ["https://github.com/Neargye/magic_enum/archive/v0.7.0.tar.gz"],
    ),
    com_github_arun11299_cpp_jwt = dict(
        sha256 = "6dbf93969ec48d97ecb6c157014985846df8c01995a0011c21f4e2c146594922",
        strip_prefix = "cpp-jwt-1.1.1",
        urls = ["https://github.com/arun11299/cpp-jwt/archive/v1.1.1.tar.gz"],
    ),
    # April 21, 2020.
    com_github_cyan4973_xxhash = dict(
        sha256 = "952ebbf5b11fbf59ae5d760a562d1e9112278f244340ad7714e8556cbe54f7f7",
        strip_prefix = "xxHash-0.7.3",
        urls = ["https://github.com/Cyan4973/xxHash/archive/v0.7.3.tar.gz"],
    ),
    com_github_nlohmann_json = dict(
        sha256 = "87b5884741427220d3a33df1363ae0e8b898099fbc59f1c451113f6732891014",
        urls = ["https://github.com/nlohmann/json/releases/download/v3.7.3/include.zip"],
    ),
    com_github_badaix_jsonrpcpp = dict(
        sha256 = "ebfe924cd7cfa5ff314de217f19d18c6bbfbd85c826765bd52f611f865922ae8",
        strip_prefix = "jsonrpcpp-1.3.2",
        urls = ["https://github.com/badaix/jsonrpcpp/archive/v1.3.2.tar.gz"],
    ),
    # August 18, 2020.
    org_tensorflow = dict(
        sha256 = "2595a5c401521f20a2734c4e5d54120996f8391f00bb62a57267d930bce95350",
        strip_prefix = "tensorflow-2.3.0",
        urls = ["https://github.com/tensorflow/tensorflow/archive/v2.3.0.tar.gz"],
    ),
    io_bazel_rules_closure = dict(
        sha256 = "5b00383d08dd71f28503736db0500b6fb4dda47489ff5fc6bed42557c07c6ba9",
        strip_prefix = "rules_closure-308b05b2419edb5c8ee0471b67a40403df940149",
        urls = [
            "https://github.com/bazelbuild/rules_closure/archive/308b05b2419edb5c8ee0471b67a40403df940149.tar.gz",
        ],
    ),
    # August 19, 2020.
    com_github_google_sentencepiece = dict(
        sha256 = "6e9863851e6277862083518cc9f96211f334215d596fc8c65e074d564baeef0c",
        strip_prefix = "sentencepiece-0.1.92",
        urls = ["https://github.com/google/sentencepiece/archive/v0.1.92.tar.gz"],
    ),
    rules_python = dict(
        urls = ["https://github.com/bazelbuild/rules_python/releases/download/0.1.0/rules_python-0.1.0.tar.gz"],
        sha256 = "b6d46438523a3ec0f3cead544190ee13223a52f6a6765a29eae7b7cc24cc83a0",
    ),
)
