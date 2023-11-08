# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

REPOSITORY_LOCATIONS = dict(
    bazel_gazelle = dict(
        sha256 = "29218f8e0cebe583643cbf93cae6f971be8a2484cdcfa1e45057658df8d54002",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.32.0/bazel-gazelle-v0.32.0.tar.gz",
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.32.0/bazel-gazelle-v0.32.0.tar.gz",
        ],
    ),
    bazel_skylib = dict(
        sha256 = "f7be3474d42aae265405a592bb7da8e171919d74c16f082a5457840f06054728",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.2.1/bazel-skylib-1.2.1.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.2.1/bazel-skylib-1.2.1.tar.gz",
        ],
    ),
    # Must be called boringssl to make sure the deps pick it up correctly.
    boringssl = dict(
        sha256 = "d11f382c25a3bea34ad8761d57828971c8b06e230ad99e1cbfd4253c419f4f9a",
        strip_prefix = "boringssl-7b00d84b025dff0c392c2df5ee8aa6d3c63ad539",
        urls = ["https://github.com/google/boringssl/" +
                "archive/7b00d84b025dff0c392c2df5ee8aa6d3c63ad539.tar.gz"],
    ),
    com_github_antlr_antlr4 = dict(
        urls = ["https://github.com/antlr/antlr4/archive/refs/tags/4.11.1.tar.gz"],
        strip_prefix = "antlr4-4.11.1",
        sha256 = "81f87f03bb83b48da62e4fc8bfdaf447efb9fb3b7f19eb5cbc37f64e171218cf",
    ),
    com_github_antlr_grammars_v4 = dict(
        urls = ["https://github.com/antlr/grammars-v4/archive/e53d7a1228505bfc80d8637808ef60e7eea92cc2.tar.gz"],
        strip_prefix = "grammars-v4-e53d7a1228505bfc80d8637808ef60e7eea92cc2",
        sha256 = "9858e4a9944cac85830e6cf8edd9d567227af96d8b75f0b31accc525ec842c30",
    ),
    com_github_apache_arrow = dict(
        sha256 = "487ae884d035d9c8bbc052199268e6259d22cf502ee976e02661ee3f8e9468c0",
        strip_prefix = "arrow-ecbb9de0b4c8739347f7ffa9e7aee7e46926bbab",
        urls = ["https://github.com/pixie-io/arrow/archive/ecbb9de0b4c8739347f7ffa9e7aee7e46926bbab.tar.gz"],
    ),
    com_github_ariafallah_csv_parser = dict(
        sha256 = "c722047128c97b7a3f38d0c320888d905692945e4a96b6ebd6d208686764644a",
        strip_prefix = "csv-parser-e3c1207f4de50603a4946dc5daa0633ce31a9257",
        urls = ["https://github.com/AriaFallah/csv-parser/archive/e3c1207f4de50603a4946dc5daa0633ce31a9257.tar.gz"],
    ),
    com_github_arun11299_cpp_jwt = dict(
        sha256 = "6dbf93969ec48d97ecb6c157014985846df8c01995a0011c21f4e2c146594922",
        strip_prefix = "cpp-jwt-1.1.1",
        urls = ["https://github.com/arun11299/cpp-jwt/archive/refs/tags/v1.1.1.tar.gz"],
    ),
    com_github_bazelbuild_buildtools = dict(
        sha256 = "977a0bd4593c8d4c8f45e056d181c35e48aa01ad4f8090bdb84f78dca42f47dc",
        strip_prefix = "buildtools-6.1.2",
        urls = ["https://github.com/bazelbuild/buildtools/archive/refs/tags/v6.1.2.tar.gz"],
    ),
    com_github_cameron314_concurrentqueue = dict(
        sha256 = "eb37336bf9ae59aca7b954db3350d9b30d1cab24b96c7676f36040aa76e915e8",
        strip_prefix = "concurrentqueue-1.0.3",
        urls = ["https://github.com/cameron314/concurrentqueue/archive/refs/tags/v1.0.3.tar.gz"],
    ),
    com_github_cyan4973_xxhash = dict(
        sha256 = "952ebbf5b11fbf59ae5d760a562d1e9112278f244340ad7714e8556cbe54f7f7",
        strip_prefix = "xxHash-0.7.3",
        urls = ["https://github.com/Cyan4973/xxHash/archive/refs/tags/v0.7.3.tar.gz"],
    ),
    com_github_derrickburns_tdigest = dict(
        sha256 = "e420c7f9c73fe2af59ab69f302ea8279ec41ae3d241b749277761fdc2e8abfd7",
        strip_prefix = "tdigest-85e0f70092460e60236821db4c25143768d3da12",
        urls = ["https://github.com/pixie-io/tdigest/archive/85e0f70092460e60236821db4c25143768d3da12.tar.gz"],
    ),
    com_github_fmeum_rules_meta = dict(
        sha256 = "ed3ed909e6e3f34a11d7c2adcc461535975a875fe434719540a4e6f63434a866",
        strip_prefix = "rules_meta-0.0.4",
        urls = [
            "https://github.com/fmeum/rules_meta/archive/refs/tags/v0.0.4.tar.gz",
        ],
    ),
    com_github_gflags_gflags = dict(
        sha256 = "9e1a38e2dcbb20bb10891b5a171de2e5da70e0a50fff34dd4b0c2c6d75043909",
        strip_prefix = "gflags-524b83d0264cb9f1b2d134c564ef1aa23f207a41",
        urls = ["https://github.com/gflags/gflags/archive/524b83d0264cb9f1b2d134c564ef1aa23f207a41.tar.gz"],
    ),
    com_github_google_glog = dict(
        sha256 = "95dc9dd17aca4e12e2cb18087a5851001f997682f5f0d0c441a5be3b86f285bd",
        strip_prefix = "glog-bc1fada1cf63ad12aee26847ab9ed4c62cffdcf9",
        # We cannot use the last released version due to https://github.com/google/glog/pull/706
        # Once there is a realease that includes that fix, we can switch to a released version.
        urls = ["https://github.com/google/glog/archive/bc1fada1cf63ad12aee26847ab9ed4c62cffdcf9.tar.gz"],
    ),
    com_github_gperftools_gperftools = dict(
        sha256 = "ea566e528605befb830671e359118c2da718f721c27225cbbc93858c7520fee3",
        strip_prefix = "gperftools-2.9.1",
        urls = ["https://github.com/gperftools/gperftools/releases/download/gperftools-2.9.1/gperftools-2.9.1.tar.gz"],
    ),
    com_github_grpc_grpc = dict(
        sha256 = "b55696fb249669744de3e71acc54a9382bea0dce7cd5ba379b356b12b82d4229",
        strip_prefix = "grpc-1.51.1",
        urls = ["https://github.com/grpc/grpc/archive/refs/tags/v1.51.1.tar.gz"],
    ),
    # August 19, 2020.
    com_github_google_sentencepiece = dict(
        sha256 = "1c0bd83e03f71a10fc934b7ce996e327488b838587f03159fd392c77c7701389",
        strip_prefix = "sentencepiece-0.1.95",
        urls = ["https://github.com/google/sentencepiece/archive/refs/tags/v0.1.95.tar.gz"],
    ),
    com_github_libarchive_libarchive = dict(
        sha256 = "c676146577d989189940f1959d9e3980d28513d74eedfbc6b7f15ea45fe54ee2",
        strip_prefix = "libarchive-3.6.1",
        urls = ["https://github.com/libarchive/libarchive/releases/download/v3.6.1/libarchive-3.6.1.tar.gz"],
    ),
    com_github_h2o_picohttpparser = dict(
        sha256 = "cb47971984d77dc81ed5684d51d668a7bc7804d3b7814a3072c2187dfa37a013",
        strip_prefix = "picohttpparser-1d2b8a184e7ebe6651c30dcede37ba1d89691351",
        urls = ["https://github.com/h2o/picohttpparser/archive/1d2b8a184e7ebe6651c30dcede37ba1d89691351.tar.gz"],
    ),
    com_github_iovisor_bcc = dict(
        sha256 = "38f07777a214582a20a198b535691459d882a1e5da80057ff30f2ee27a53055c",
        strip_prefix = "bcc-0171a363859b4a96c23543c06ef67517ddc3e789",
        urls = [
            "https://github.com/pixie-io/bcc/archive/0171a363859b4a96c23543c06ef67517ddc3e789.tar.gz",
        ],
    ),
    com_github_iovisor_bpftrace = dict(
        sha256 = "92138b530a222efbe7506b337b91694f994d2bee1723263cb740766297be2156",
        strip_prefix = "bpftrace-460adf093c226a3013ff389cf9a2d84834018b9b",
        urls = [
            "https://github.com/pixie-io/bpftrace/archive/460adf093c226a3013ff389cf9a2d84834018b9b.tar.gz",
        ],
    ),
    com_github_jupp0r_prometheus_cpp = dict(
        sha256 = "b4eff62bcdba10efd6210b9fa8a5b2505ad8ea6c211968be79aeb2c4c2f97338",
        strip_prefix = "prometheus-cpp-81e208c250748657f1d5dab247e82c4429a931af",
        urls = ["https://github.com/jupp0r/prometheus-cpp/archive/81e208c250748657f1d5dab247e82c4429a931af.tar.gz"],
    ),
    com_github_libbpf_libbpf = dict(
        sha256 = "3d6afde67682c909e341bf194678a8969f17628705af25f900d5f68bd299cb03",
        strip_prefix = "libbpf-1.0.1",
        urls = [
            "https://github.com/libbpf/libbpf/archive/refs/tags/v1.0.1.tar.gz",
        ],
    ),
    com_github_libuv_libuv = dict(
        sha256 = "371e5419708f6aaeb8656671f89400b92a9bba6443369af1bb70bcd6e4b3c764",
        strip_prefix = "libuv-1.42.0",
        urls = ["https://github.com/libuv/libuv/archive/refs/tags/v1.42.0.tar.gz"],
    ),
    com_github_mongodb_mongo_c_driver = dict(
        sha256 = "fdb9de045efabedf67b0ac8d7e77006705b22d954579567af75d440fd0a2c5e1",
        strip_prefix = "mongo-c-driver-1.24.0",
        urls = ["https://github.com/mongodb/mongo-c-driver/archive/refs/tags/1.24.0.tar.gz"],
    ),
    com_github_nats_io_natsc = dict(
        sha256 = "16e700d912034faefb235a955bd920cfe4d449a260d0371b9694d722eb617ae1",
        strip_prefix = "nats.c-3.3.0",
        urls = ["https://github.com/nats-io/nats.c/archive/refs/tags/v3.3.0.tar.gz"],
    ),
    com_github_neargye_magic_enum = dict(
        sha256 = "4fe6627407a656d0d73879c0346b251ccdcfb718c37bef5410ba172c7c7d5f9a",
        strip_prefix = "magic_enum-0.7.0",
        urls = ["https://github.com/Neargye/magic_enum/archive/refs/tags/v0.7.0.tar.gz"],
    ),
    com_github_nlohmann_json = dict(
        sha256 = "87b5884741427220d3a33df1363ae0e8b898099fbc59f1c451113f6732891014",
        urls = ["https://github.com/nlohmann/json/releases/download/v3.7.3/include.zip"],
    ),
    com_github_opentelemetry_proto = dict(
        urls = ["https://github.com/open-telemetry/opentelemetry-proto/archive/refs/tags/v0.10.0.tar.gz"],
        strip_prefix = "opentelemetry-proto-0.10.0",
        sha256 = "f1004a49f40d7acb43e86b1fd95f73e80c778acb163e309bba86f0cbd7fa8a71",
    ),
    com_github_packetzero_dnsparser = dict(
        sha256 = "bdf6c7f56f33725c1c32e672a4779576fb639dd2df565115778eb6be48296431",
        strip_prefix = "dnsparser-77398ffc200765db1cea9000d9f550ea99a29f7b",
        urls = ["https://github.com/pixie-io/dnsparser/archive/77398ffc200765db1cea9000d9f550ea99a29f7b.tar.gz"],
    ),
    com_github_pgcodekeeper_pgcodekeeper = dict(
        urls = ["https://github.com/pgcodekeeper/pgcodekeeper/archive/refs/tags/v5.11.3.tar.gz"],
        strip_prefix = "pgcodekeeper-5.11.3",
        sha256 = "b80d88f447566733f887a2c21ad6072751297459e79fa5acfc99e5db3a9418a1",
    ),
    com_github_rlyeh_sole = dict(
        sha256 = "70dbd71f2601963684195f4c7d8a1c2d45a0d53114bc4d06f8cebe6d3d3ffa69",
        strip_prefix = "sole-95612e5cda1accc0369a51edfe0f32bfb4bee2a0",
        urls = ["https://github.com/r-lyeh-archived/sole/archive/95612e5cda1accc0369a51edfe0f32bfb4bee2a0.tar.gz"],
    ),
    com_github_serge1_elfio = dict(
        sha256 = "17ed6c4ca076be0ba6c3b1dcdae8c7aae9029f70a470be5fbc58526c96b9df05",
        strip_prefix = "ELFIO-98d87a350f2384ce22b5dc72c79312a6854d88d4",
        urls = ["https://github.com/pixie-io/ELFIO/archive/98d87a350f2384ce22b5dc72c79312a6854d88d4.tar.gz"],
    ),
    com_github_simdutf_simdutf = dict(
        urls = ["https://github.com/simdutf/simdutf/archive/refs/tags/v4.0.3.zip"],
        strip_prefix = "simdutf-4.0.3",
        sha256 = "a7f468fdb4355259ed81d4e97434abcea85098986592c4f6012b9d208b8734fe",
    ),
    com_github_tencent_rapidjson = dict(
        sha256 = "4a34a0c21794f067afca6c9809971f0bd77a1d1834c4dc53bdd09e4ab4d05ce4",
        strip_prefix = "rapidjson-f56928de85d56add3ca6ae7cf7f119a42ee1585b",
        urls = ["https://github.com/Tencent/rapidjson/archive/f56928de85d56add3ca6ae7cf7f119a42ee1585b.tar.gz"],
    ),
    com_github_thoughtspot_threadstacks = dict(
        sha256 = "e54d4c3cd5af3cc136cc952c1ef77cd90b41133cd61140d8488e14c6d6f795e9",
        strip_prefix = "threadstacks-94adbe26c4aaf9ca945fd7936670d40ec6f228fb",
        urls = ["https://github.com/pixie-io/threadstacks/archive/94adbe26c4aaf9ca945fd7936670d40ec6f228fb.tar.gz"],
    ),
    com_github_USCiLab_cereal = dict(
        urls = ["https://github.com/USCiLab/cereal/archive/refs/tags/v1.3.1.tar.gz"],
        strip_prefix = "cereal-1.3.1",
        sha256 = "65ea6ddda98f4274f5c10fb3e07b2269ccdd1e5cbb227be6a2fd78b8f382c976",
    ),
    com_github_uriparser_uriparser = dict(
        urls = ["https://github.com/uriparser/uriparser/releases/download/uriparser-0.9.6/uriparser-0.9.6.tar.gz"],
        sha256 = "10e6f90d359c1087c45f907f95e527a8aca84422251081d1533231e031a084ff",
        strip_prefix = "uriparser-0.9.6",
    ),
    com_github_vinzenz_libpypa = dict(
        sha256 = "a2425b4336d4dea21124b87ce51fa6f67c212f4b5b1496af4fae7cba73724efc",
        strip_prefix = "libpypa-eba8ec485a6c5e566d0d7a0716a06c91837c9d2f",
        urls = ["https://github.com/pixie-io/libpypa/archive/eba8ec485a6c5e566d0d7a0716a06c91837c9d2f.tar.gz"],
    ),
    com_google_absl = dict(
        sha256 = "91ac87d30cc6d79f9ab974c51874a704de9c2647c40f6932597329a282217ba8",
        strip_prefix = "abseil-cpp-20220623.1",
        urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20220623.1.tar.gz"],
    ),
    com_google_benchmark = dict(
        sha256 = "3aff99169fa8bdee356eaa1f691e835a6e57b1efeadb8a0f9f228531158246ac",
        strip_prefix = "benchmark-1.7.0",
        urls = ["https://github.com/google/benchmark/archive/refs/tags/v1.7.0.tar.gz"],
    ),
    com_google_double_conversion = dict(
        sha256 = "3dbcdf186ad092a8b71228a5962009b5c96abde9a315257a3452eb988414ea3b",
        strip_prefix = "double-conversion-3.2.0",
        urls = ["https://github.com/google/double-conversion/archive/refs/tags/v3.2.0.tar.gz"],
    ),
    com_google_farmhash = dict(
        sha256 = "09b5da9eaa7c7f4f073053c1c6c398e320ca917e74e8f366fd84679111e87216",
        strip_prefix = "farmhash-2f0e005b81e296fa6963e395626137cf729b710c",
        urls = ["https://github.com/google/farmhash/archive/2f0e005b81e296fa6963e395626137cf729b710c.tar.gz"],
    ),
    com_google_flatbuffers = dict(
        sha256 = "e2dc24985a85b278dd06313481a9ca051d048f9474e0f199e372fea3ea4248c9",
        strip_prefix = "flatbuffers-2.0.6",
        urls = ["https://github.com/google/flatbuffers/archive/refs/tags/v2.0.6.tar.gz"],
    ),
    com_google_googletest = dict(
        sha256 = "81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2",
        strip_prefix = "googletest-release-1.12.1",
        urls = ["https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz"],
    ),
    com_google_protobuf = dict(
        sha256 = "63c5539a8506dc6bccd352a857cea106e0a389ce047a3ff0a78fe3f8fede410d",
        strip_prefix = "protobuf-24487dd1045c7f3d64a21f38a3f0c06cc4cf2edb",
        urls = [
            "https://storage.googleapis.com/grpc-bazel-mirror/github.com/protocolbuffers/protobuf/archive/24487dd1045c7f3d64a21f38a3f0c06cc4cf2edb.tar.gz",
            "https://github.com/protocolbuffers/protobuf/archive/24487dd1045c7f3d64a21f38a3f0c06cc4cf2edb.tar.gz",
        ],
    ),
    com_google_protobuf_javascript = dict(
        sha256 = "35bca1729532b0a77280bf28ab5937438e3dcccd6b31a282d9ae84c896b6f6e3",
        strip_prefix = "protobuf-javascript-3.21.2",
        urls = [
            "https://github.com/protocolbuffers/protobuf-javascript/archive/refs/tags/v3.21.2.tar.gz",
        ],
    ),
    com_googlesource_code_re2 = dict(
        urls = ["https://github.com/google/re2/archive/refs/tags/2021-08-01.tar.gz"],
        strip_prefix = "re2-2021-08-01",
        sha256 = "cd8c950b528f413e02c12970dce62a7b6f37733d7f68807e73a2d9bc9db79bc8",
    ),
    com_intel_tbb = dict(
        sha256 = "91eab849ab1442db72317f8c968c5a1010f8546ca35f26086201262096c8a8a9",
        strip_prefix = "oneTBB-e6104c9599f7f10473caf545199f7468c0a8e52f",
        urls = ["https://github.com/oneapi-src/oneTBB/archive/e6104c9599f7f10473caf545199f7468c0a8e52f.tar.gz"],
    ),
    com_llvm_clang_15 = dict(
        sha256 = "13924fc595146bf2480f8c853bb1b995ef511ee2d7eb6f5c19ab429e31caaf98",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/clang-min-15.0-pl12.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/clang-min-15.0-pl12.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_lib_x86_64_glibc_host = dict(
        sha256 = "bd71e0fc496c10804840827161f183f2430304d2f272b622a3c101744c4799d2",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/llvm-15.0-pl12-libstdc%2B%2B.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/llvm-15.0-pl12-libstdc%2B%2B.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_lib_libcpp_x86_64_glibc_host = dict(
        sha256 = "f7c126bced889730b4b7f63a3c4290895ce8943d87b279558a6e7ed835eb1ef9",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/llvm-15.0-pl12-libcxx.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/llvm-15.0-pl12-libcxx.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_lib_libcpp_x86_64_glibc_host_asan = dict(
        sha256 = "c50fd3b3c9b359f1112646aed544f274d28372cb464bc3e1ad11e25c4da6ed3c",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/llvm-15.0-pl12-libcxx-asan.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/llvm-15.0-pl12-libcxx-asan.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_lib_libcpp_x86_64_glibc_host_msan = dict(
        sha256 = "b176a2df6f31f07bc4c2675c82523cf290cc29a037af33798ad6786b161ef570",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/llvm-15.0-pl12-libcxx-msan.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/llvm-15.0-pl12-libcxx-msan.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_lib_libcpp_x86_64_glibc_host_tsan = dict(
        sha256 = "13ba581991271197a33aa35576d62ffa687e56b4799aadb8b91e4a30e1ebe57f",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/llvm-15.0-pl12-libcxx-tsan.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/llvm-15.0-pl12-libcxx-tsan.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_lib_x86_64_glibc2_36 = dict(
        sha256 = "22dbfc1fe13c4809b7df94360f45d7ce2064bcd954ab642a7053c79f95fe21ad",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/llvm-15.0-pl12-libstdc++-x86_64-sysroot.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/llvm-15.0-pl12-libstdc++-x86_64-sysroot.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_lib_libcpp_x86_64_glibc2_36 = dict(
        sha256 = "fba4ba91f794809fb74d8736a0b281589eb3faa1064a55b03f18fabea22d4318",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/llvm-15.0-pl12-libcxx-x86_64-sysroot.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/llvm-15.0-pl12-libcxx-x86_64-sysroot.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_lib_aarch64_glibc2_36 = dict(
        sha256 = "a38c075733a18e59d41994e18ea0e4944779d8d971dc1f825dc52a81253c602e",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/llvm-15.0-pl12-libstdc++-aarch64-sysroot.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/llvm-15.0-pl12-libstdc++-aarch64-sysroot.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_lib_libcpp_aarch64_glibc2_36 = dict(
        sha256 = "d8583cbd55a66c6ea2d53268b3ae4829dae6d9e16e5d8040646f2cf1b7d8cdde",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/llvm-15.0-pl12-libcxx-aarch64-sysroot.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/llvm-15.0-pl12-libcxx-aarch64-sysroot.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_libcxx_x86_64_glibc_host = dict(
        sha256 = "12321054fa99b953ffde02a1b100e8ba1f16b3769f2db7623e19c0cac9d2185a",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/libcxx-15.0-pl12.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/libcxx-15.0-pl12.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_libcxx_x86_64_glibc2_36 = dict(
        sha256 = "6f0e66ba3c580f9cd2950f6c978d87234e876b8de3cdb8b3f0f827b57bdf2b64",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/libcxx-15.0-pl12-x86_64-sysroot.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/libcxx-15.0-pl12-x86_64-sysroot.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_llvm_libcxx_aarch64_glibc2_36 = dict(
        sha256 = "f5d4b52a9c142f28f9e88e09ebd52c91f2749417e6dcda28e66c8b20688b9b1a",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F15.0-pl12/libcxx-15.0-pl12-aarch64-sysroot.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl12/libcxx-15.0-pl12-aarch64-sysroot.tar.gz",
        ],
        manual_license_name = "llvm/llvm-project",
    ),
    com_oracle_openjdk_18 = dict(
        sha256 = "3bfdb59fc38884672677cebca9a216902d87fe867563182ae8bc3373a65a2ebd",
        strip_prefix = "jdk-18.0.2.1",
        urls = [
            "https://download.java.net/java/GA/jdk18.0.2.1/db379da656dc47308e138f21b33976fa/1/GPL/openjdk-18.0.2.1_linux-x64_bin.tar.gz",
        ],
        manual_license_name = "oracle/openjdk",
    ),
    io_bazel_rules_closure = dict(
        sha256 = "5b00383d08dd71f28503736db0500b6fb4dda47489ff5fc6bed42557c07c6ba9",
        strip_prefix = "rules_closure-308b05b2419edb5c8ee0471b67a40403df940149",
        urls = ["https://github.com/bazelbuild/rules_closure/archive/308b05b2419edb5c8ee0471b67a40403df940149.tar.gz"],
    ),
    io_bazel_rules_docker = dict(
        sha256 = "b1e80761a8a8243d03ebca8845e9cc1ba6c82ce7c5179ce2b295cd36f7e394bf",
        urls = ["https://github.com/bazelbuild/rules_docker/releases/download/v0.25.0/rules_docker-v0.25.0.tar.gz"],
    ),
    io_bazel_rules_go = dict(
        sha256 = "278b7ff5a826f3dc10f04feaf0b70d48b68748ccd512d7f98bf442077f043fe3",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.41.0/rules_go-v0.41.0.zip",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.41.0/rules_go-v0.41.0.zip",
        ],
    ),
    io_bazel_rules_k8s = dict(
        sha256 = "ce5b9bc0926681e2e7f2147b49096f143e6cbc783e71bc1d4f36ca76b00e6f4a",
        strip_prefix = "rules_k8s-0.7",
        urls = [
            "https://github.com/bazelbuild/rules_k8s/archive/refs/tags/v0.7.tar.gz",
        ],
    ),
    io_bazel_rules_scala = dict(
        sha256 = "6e9191363357d30b144e7306fec74deea2c7f1de63f3ed32028838116c239e8a",
        urls = [
            "https://github.com/bazelbuild/rules_scala/archive/4ba3780fcba8d26980daff4639abc6f18517308b.tar.gz",
        ],
        strip_prefix = "rules_scala-4ba3780fcba8d26980daff4639abc6f18517308b",
    ),
    org_tensorflow = dict(
        sha256 = "99c732b92b1b37fc243a559e02f9aef5671771e272758aa4aec7f34dc92dac48",
        strip_prefix = "tensorflow-2.11.0",
        urls = ["https://github.com/tensorflow/tensorflow/archive/refs/tags/v2.11.0.tar.gz"],
    ),
    org_libc_musl = dict(
        sha256 = "7d5b0b6062521e4627e099e4c9dc8248d32a30285e959b7eecaa780cf8cfd4a4",
        strip_prefix = "musl-1.2.3",
        urls = ["http://musl.libc.org/releases/musl-1.2.3.tar.gz"],
        manual_license_name = "libc/musl",
    ),
    rules_foreign_cc = dict(
        sha256 = "6041f1374ff32ba711564374ad8e007aef77f71561a7ce784123b9b4b88614fc",
        strip_prefix = "rules_foreign_cc-0.8.0",
        urls = ["https://github.com/bazelbuild/rules_foreign_cc/archive/refs/tags/0.8.0.tar.gz"],
    ),
    rules_python = dict(
        sha256 = "cdf6b84084aad8f10bf20b46b77cb48d83c319ebe6458a18e9d2cebf57807cdd",
        strip_prefix = "rules_python-0.8.1",
        urls = ["https://github.com/bazelbuild/rules_python/archive/refs/tags/0.8.1.tar.gz"],
    ),
    rules_jvm_external = dict(
        urls = ["https://github.com/bazelbuild/rules_jvm_external/archive/refs/tags/5.2.tar.gz"],
        sha256 = "c9ae901381ae7f7eca08aed96caeb542f96c5449052db9c9d27274a8dc154cdf",
        strip_prefix = "rules_jvm_external-5.2",
    ),
    rules_pkg = dict(
        sha256 = "eea0f59c28a9241156a47d7a8e32db9122f3d50b505fae0f33de6ce4d9b61834",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.8.0/rules_pkg-0.8.0.tar.gz",
            "https://github.com/bazelbuild/rules_pkg/releases/download/0.8.0/rules_pkg-0.8.0.tar.gz",
        ],
    ),
    unix_cc_toolchain_config = dict(
        sha256 = "2c1d60ef4d586909f138c28409780e102e2ebd619e7d462ded26dce43a8f9ffb",
        urls = [
            "https://raw.githubusercontent.com/bazelbuild/bazel/5.3.1/tools/cpp/unix_cc_toolchain_config.bzl",
        ],
    ),
    # GRPC and Protobuf pick different versions. Pick the newer one.
    upb = dict(
        sha256 = "017a7e8e4e842d01dba5dc8aa316323eee080cd1b75986a7d1f94d87220e6502",
        strip_prefix = "upb-e4635f223e7d36dfbea3b722a4ca4807a7e882e2",
        urls = [
            "https://storage.googleapis.com/grpc-bazel-mirror/github.com/protocolbuffers/upb/archive/e4635f223e7d36dfbea3b722a4ca4807a7e882e2.tar.gz",
            "https://github.com/protocolbuffers/upb/archive/e4635f223e7d36dfbea3b722a4ca4807a7e882e2.tar.gz",
        ],
    ),
    cpuinfo = dict(
        sha256 = "18eca9bc8d9c4ce5496d0d2be9f456d55cbbb5f0639a551ce9c8bac2e84d85fe",
        strip_prefix = "cpuinfo-5e63739504f0f8e18e941bd63b2d6d42536c7d90",
        urls = ["https://github.com/pytorch/cpuinfo/archive/5e63739504f0f8e18e941bd63b2d6d42536c7d90.tar.gz"],
    ),
)

# To modify one of the forked repos below:
#  1. Make the changes to the repo and push the changes to the `pixie` on github.
#  2. Update the commit below to point to the commit hash of the new `pixie` branch.
#
# To use a local repo for local development, change `remote` to a file path.
#   ex: remote = "/home/user/src/pixie-io/bcc"
# Then change the local repo, commit the change, and replace `commit` with your new commit.
# See LOCAL_REPOSITORY_LOCATIONS for an alternative approach.
GIT_REPOSITORY_LOCATIONS = dict(
    com_github_apangin_jattach = dict(
        remote = "https://github.com/pixie-io/jattach.git",
        commit = "fa36a4fa141b4e9486b9126640d54a94c1d36fce",
        shallow_since = "1638898188 -0800",
    ),
)

# To use a local repo for local development, update the path to point to your local repo.
#   ex: path = "/home/user/pixie-io/bcc"
# then uncomment the lines with `_local_repo(name_of_repo_you_care_about, ...)` in `repositories.bzl` and
# comment out the corresponding lines with `_bazel_repo(name_of_repo_you_care_about, ...)`.
# Note that if you do this, you have to handle the building of these repos' artifacts yourself.
# See `bazel/external/local_dev/{bcc,bpftrace}.BUILD` for the cmake commands for building these repos.
#
# WARNING: doing this has some downsides, so don't do it for production builds. For instance,
# cflags and other settings set by bazel (eg -O3) won't be used, since you have to do the building manually.
LOCAL_REPOSITORY_LOCATIONS = dict(
    com_github_iovisor_bcc = dict(
        path = "/home/user/pixie-io/bcc",
    ),
    com_github_iovisor_bpftrace = dict(
        path = "/home/user/pixie-io/bpftrace",
    ),
)
