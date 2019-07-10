REPOSITORY_LOCATIONS = dict(
    bazel_gazelle = dict(
        sha256 = "3c681998538231a2d24d0c07ed5a7658cb72bfb5fd4bf9911157c0e9ac6a2687",
        urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/0.17.0/bazel-gazelle-0.17.0.tar.gz"],
    ),
    io_bazel_rules_go = dict(
        sha256 = "a82a352bffae6bee4e95f68a8d80a70e87f42c4741e6a448bec11998fcc82329",
        urls = ["https://github.com/bazelbuild/rules_go/releases/download/0.18.5/rules_go-0.18.5.tar.gz"],
    ),
    com_github_bazelbuild_buildtools = dict(
        sha256 = "9da0e2911d78554be7d926d6e7a360d62856951da052108ee0772258b2b5c800",
        strip_prefix = "buildtools-db073457c5a56d810e46efc18bb93a4fd7aa7b5e",
        urls = ["https://github.com/bazelbuild/buildtools/archive/db073457c5a56d810e46efc18bb93a4fd7aa7b5e.tar.gz"],
    ),
    com_google_benchmark = dict(
        sha256 = "3c6a165b6ecc948967a1ead710d4a181d7b0fbcaa183ef7ea84604994966221a",
        strip_prefix = "benchmark-1.5.0",
        urls = ["https://github.com/google/benchmark/archive/v1.5.0.tar.gz"],
    ),
    io_bazel_rules_skylib = dict(
        sha256 = "2ea8a5ed2b448baf4a6855d3ce049c4c452a6470b1efd1504fdb7c1c134d220a",
        strip_prefix = "bazel-skylib-0.8.0",
        urls = ["https://github.com/bazelbuild/bazel-skylib/archive/0.8.0.tar.gz"],
    ),
    io_bazel_rules_docker = dict(
        sha256 = "aed1c249d4ec8f703edddf35cbe9dfaca0b5f5ea6e4cd9e83e99f3b0d1136c3d",
        strip_prefix = "rules_docker-0.7.0",
        urls = ["https://github.com/bazelbuild/rules_docker/archive/v0.7.0.tar.gz"],
    ),
    io_bazel_toolchains = dict(
        sha256 = "e76afea244b1767e19fb38e1f1be448ebdf48d52ade0b3687c5794d8a1362fe8",
        strip_prefix = "bazel-toolchains-0.26.3",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-toolchains/archive/0.26.3.tar.gz",
            "https://github.com/bazelbuild/bazel-toolchains/archive/0.26.3.tar.gz",
        ],
    ),
    com_google_googletest = dict(
        sha256 = "9bf1fe5182a604b4135edc1a425ae356c9ad15e9b23f9f12a02e80184c3a249c",
        strip_prefix = "googletest-release-1.8.1",
        urls = ["https://github.com/google/googletest/archive/release-1.8.1.tar.gz"],
    ),
    # WHEN UPDATING EITHER GRPC OR BORING SSL please make sure that the version of
    # boringssl matches the version used by GRPC.
    # TODO(zasgar): Figure out how we can make GRPC using our boringssl as a dep.
    com_github_grpc_grpc = dict(
        sha256 = "50747c8939c535b1059f19534de263eb9b7570b5347390fb24b0bbce8763e9a4",
        strip_prefix = "grpc-1.21.3",
        urls = ["https://github.com/grpc/grpc/archive/v1.21.3.tar.gz"],
    ),
    com_google_boringssl = dict(
        sha256 = "2b18e1c1ad15cc180529ababde8a62885ac35005131e9a797cdaf0e07d76a767",
        strip_prefix = "boringssl-afc30d43eef92979b05776ec0963c9cede5fb80f",
        urls = ["https://github.com/google/boringssl/" +
                "archive/afc30d43eef92979b05776ec0963c9cede5fb80f.tar.gz"],
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
    com_github_rlyeh_sole = dict(
        sha256 = "0e2d2d280e6847b3301c7302b7924e2841f517985cb189ce0fb94aa9fb5a17c7",
        strip_prefix = "sole-653a25ad03775d7e0a2d50142160795723915ba6",
        urls = ["https://github.com/r-lyeh-archived/sole/archive/653a25ad03775d7e0a2d50142160795723915ba6.tar.gz"],
    ),
    com_google_absl = dict(
        sha256 = "b42bbd55f6e8aec0bd03a82299f172f78d8bc196a5ff0bc9da3a9cc87dc749cc",
        strip_prefix = "abseil-cpp-44efe96dfca674a17b45ca53fc77fb69f1e29bf4",
        urls = ["https://github.com/abseil/abseil-cpp/archive/44efe96dfca674a17b45ca53fc77fb69f1e29bf4.tar.gz"],
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
        sha256 = "fc22de09b56c68bf4e0463e33352f0d7622eb9500ba93af453b7d2d66b5d6be9",
        strip_prefix = "rapidjson-7484e06c589873e1ed80382d262087e4fa80fb63",
        urls = ["https://github.com/Tencent/rapidjson/archive/7484e06c589873e1ed80382d262087e4fa80fb63.tar.gz"],
    ),
    com_github_ariafallah_csv_parser = dict(
        sha256 = "c722047128c97b7a3f38d0c320888d905692945e4a96b6ebd6d208686764644a",
        strip_prefix = "csv-parser-e3c1207f4de50603a4946dc5daa0633ce31a9257",
        urls = ["https://github.com/AriaFallah/csv-parser/archive/e3c1207f4de50603a4946dc5daa0633ce31a9257.tar.gz"],
    ),
    rules_foreign_cc = dict(
        sha256 = "55d490b062adc37ca70cabe8daae6324da7760a6c894d7953e6c0ef35379a769",
        strip_prefix = "rules_foreign_cc-6b2f454a41dc1d59607718224438d66ad4f4ddbc",
        # 2019-06-03
        urls = ["https://github.com/bazelbuild/rules_foreign_cc/" +
                "archive/6b2f454a41dc1d59607718224438d66ad4f4ddbc.tar.gz"],
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
        sha256 = "af1dd62ace9925f8aec73d303397c29f54611b6beef9901229dceecdbd3622ed",
        strip_prefix = "distroless-b4dfb5c3076302a873be8e413c120771c4cc2e1b",
        urls = ["https://github.com/GoogleContainerTools/distroless/" +
                "archive/b4dfb5c3076302a873be8e413c120771c4cc2e1b.tar.gz"],
    ),
    com_github_nats_io_natsc = dict(
        sha256 = "14e50dd3cf30c9839aedf7c8929f3d433c0a69af38f13f7baf5491d9ed2ac43b",
        strip_prefix = "nats.c-3b4668698b8510b8f08413a94523b05a8036d9ab",
        urls = ["https://github.com/nats-io/nats.c/archive/3b4668698b8510b8f08413a94523b05a8036d9ab.tar.gz"],
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
)
