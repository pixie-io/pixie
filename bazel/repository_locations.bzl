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
        sha256 = "501deb3d5695ab658e82f6f6f549ba681ea3ca2a5fb7911154b5aa45596183fa",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.26.0/bazel-gazelle-v0.26.0.tar.gz",
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.26.0/bazel-gazelle-v0.26.0.tar.gz",
        ],
    ),
    bazel_skylib = dict(
        sha256 = "710c2ca4b4d46250cdce2bf8f5aa76ea1f0cba514ab368f2988f70e864cfaf51",
        strip_prefix = "bazel-skylib-1.2.1",
        urls = ["https://github.com/bazelbuild/bazel-skylib/archive/refs/tags/1.2.1.tar.gz"],
    ),
    # Must be called boringssl to make sure the deps pick it up correctly.
    boringssl = dict(
        sha256 = "d11f382c25a3bea34ad8761d57828971c8b06e230ad99e1cbfd4253c419f4f9a",
        strip_prefix = "boringssl-7b00d84b025dff0c392c2df5ee8aa6d3c63ad539",
        urls = ["https://github.com/google/boringssl/" +
                "archive/7b00d84b025dff0c392c2df5ee8aa6d3c63ad539.tar.gz"],
    ),
    com_github_antlr_antlr4 = dict(
        urls = ["https://github.com/antlr/antlr4/archive/refs/tags/4.9.2.tar.gz"],
        strip_prefix = "antlr4-4.9.2",
        sha256 = "6c86ebe2f3583ac19b199e704bdff9d70379f12347f7f2f1efa38051cd9a18cf",
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
        sha256 = "d368c47bbfc055010f118efb2962987475418737e901f7782d2a966d1dc80296",
        strip_prefix = "buildtools-4.2.5",
        urls = ["https://github.com/bazelbuild/buildtools/archive/refs/tags/4.2.5.tar.gz"],
    ),
    com_github_cameron314_concurrentqueue = dict(
        sha256 = "eb37336bf9ae59aca7b954db3350d9b30d1cab24b96c7676f36040aa76e915e8",
        strip_prefix = "concurrentqueue-1.0.3",
        urls = ["https://github.com/cameron314/concurrentqueue/archive/refs/tags/v1.0.3.tar.gz"],
    ),
    com_github_cmcqueen_aes_min = dict(
        sha256 = "dd82d23976695d857924780c262952cdb12ddbb56e6bdaf5a2270dccc851d279",
        strip_prefix = "aes-min-0.3.1",
        urls = ["https://github.com/cmcqueen/aes-min/releases/download/0.3.1/aes-min-0.3.1.tar.gz"],
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
        sha256 = "d6cbf22cb5007af71b61c6be316a79397469c58c82a942552a62e708bce60964",
        strip_prefix = "grpc-1.46.3",
        urls = ["https://github.com/grpc/grpc/archive/refs/tags/v1.46.3.tar.gz"],
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
        sha256 = "0fe8e9f0ebac749db7bdc96dfc532fdf34f4f804a2103e4840a5160651ed3bf5",
        strip_prefix = "bcc-b9cc317740d943173a493f2941f716679c560fa8",
        urls = [
            "https://github.com/pixie-io/bcc/archive/b9cc317740d943173a493f2941f716679c560fa8.tar.gz",
        ],
    ),
    com_github_iovisor_bpftrace = dict(
        sha256 = "b8ebddb930aca0293f4f0f9ffaf3b550eba6e14a5ba3f0f3abbb3028873e5554",
        strip_prefix = "bpftrace-ad53050229186cb71021bf4d6617e8765c666a3c",
        urls = [
            "https://github.com/pixie-io/bpftrace/archive/ad53050229186cb71021bf4d6617e8765c666a3c.tar.gz",
        ],
    ),
    com_github_jupp0r_prometheus_cpp = dict(
        urls = ["https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/v0.13.0.tar.gz"],
        strip_prefix = "prometheus-cpp-0.13.0",
        sha256 = "5319b77d6dc73af34bc256e7b18a7e0da50c787ef6f9e32785d045428b6473cc",
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
    com_github_openssl_openssl = dict(
        sha256 = "9f54d42aed56f62889e8384895c968e24d57eae701012776d5f18fb9f2ae48b0",
        strip_prefix = "openssl-openssl-3.0.2",
        urls = ["https://github.com/openssl/openssl/archive/refs/tags/openssl-3.0.2.tar.gz"],
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
        sha256 = "ff82a1d6071cbc9c709864266210ddedecdb2b1e507ac5e7c4290ca6453e89b3",
        strip_prefix = "sole-1.0.2",
        urls = ["https://github.com/r-lyeh-archived/sole/archive/refs/tags/1.0.2.tar.gz"],
    ),
    com_github_serge1_elfio = dict(
        sha256 = "f1e2edddec556ac61705b931b5d59f1c89440442d5be522d3ae7d317b917e2d9",
        strip_prefix = "ELFIO-b8d2a419b0edf185cfd7dc49a837d8d97001a7ba",
        urls = ["https://github.com/pixie-io/ELFIO/archive/b8d2a419b0edf185cfd7dc49a837d8d97001a7ba.tar.gz"],
    ),
    com_github_simdutf_simdutf = dict(
        urls = ["https://github.com/simdutf/simdutf/archive/refs/tags/v1.0.0.tar.gz"],
        strip_prefix = "simdutf-1.0.0",
        sha256 = "a91056e53e566070068203b77a4607fec41920b923712464cf54e12a760cd0a6",
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
        sha256 = "9ddb9031798f4f8754d00fca2f1a68ecf9d0f83dfac7239af1311e4fd9a565c4",
        strip_prefix = "flatbuffers-2.0.0",
        urls = ["https://github.com/google/flatbuffers/archive/refs/tags/v2.0.0.tar.gz"],
    ),
    # To workaround issues in TF 2.9.1 bazel workspace setup, we directly load this library that tensorflow should have loaded itself.
    com_google_googleapis = dict(
        sha256 = "5bb6b0253ccf64b53d6c7249625a7e3f6c3bc6402abd52d3778bfa48258703a0",
        strip_prefix = "googleapis-2f9af297c84c55c8b871ba4495e01ade42476c92",
        urls = [
            "https://storage.googleapis.com/grpc-bazel-mirror/github.com/googleapis/googleapis/archive/2f9af297c84c55c8b871ba4495e01ade42476c92.tar.gz",
            "https://github.com/googleapis/googleapis/archive/2f9af297c84c55c8b871ba4495e01ade42476c92.tar.gz",
        ],
    ),
    com_google_googletest = dict(
        sha256 = "b4870bf121ff7795ba20d20bcdd8627b8e088f2d1dab299a031c1034eddc93d5",
        strip_prefix = "googletest-release-1.11.0",
        urls = ["https://github.com/google/googletest/archive/release-1.11.0.tar.gz"],
    ),
    com_google_protobuf = dict(
        sha256 = "bab1685f92cc4ea5b6420026eef6c7973ae96fc21f4f1a3ee626dc6ca6d77c12",
        strip_prefix = "protobuf-22d0e265de7d2b3d2e9a00d071313502e7d4cccf",
        urls = [
            "https://storage.googleapis.com/grpc-bazel-mirror/github.com/protocolbuffers/protobuf/archive/22d0e265de7d2b3d2e9a00d071313502e7d4cccf.tar.gz",
            "https://github.com/protocolbuffers/protobuf/archive/22d0e265de7d2b3d2e9a00d071313502e7d4cccf.tar.gz",
        ],
    ),
    com_googlesource_code_re2 = dict(
        urls = ["https://github.com/google/re2/archive/refs/tags/2021-08-01.tar.gz"],
        strip_prefix = "re2-2021-08-01",
        sha256 = "cd8c950b528f413e02c12970dce62a7b6f37733d7f68807e73a2d9bc9db79bc8",
    ),
    com_intel_tbb = dict(
        sha256 = "8bc2bc624fd382f5262adb62ff25cb218a6ec1a20330dc6e90f0c166f65b3b81",
        strip_prefix = "oneTBB-2021.6.0-rc1",
        urls = ["https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.6.0-rc1.tar.gz"],
    ),
    com_oracle_openjdk_18 = dict(
        sha256 = "3bfdb59fc38884672677cebca9a216902d87fe867563182ae8bc3373a65a2ebd",
        strip_prefix = "jdk-18.0.2.1",
        urls = [
            "https://download.java.net/java/GA/jdk18.0.2.1/db379da656dc47308e138f21b33976fa/1/GPL/openjdk-18.0.2.1_linux-x64_bin.tar.gz",
        ],
    ),
    io_bazel_rules_closure = dict(
        sha256 = "5b00383d08dd71f28503736db0500b6fb4dda47489ff5fc6bed42557c07c6ba9",
        strip_prefix = "rules_closure-308b05b2419edb5c8ee0471b67a40403df940149",
        urls = ["https://github.com/bazelbuild/rules_closure/archive/308b05b2419edb5c8ee0471b67a40403df940149.tar.gz"],
    ),
    io_bazel_rules_docker = dict(
        sha256 = "07ee8ca536080f5ebab6377fc6e8920e9a761d2ee4e64f0f6d919612f6ab56aa",
        strip_prefix = "rules_docker-0.25.0",
        urls = ["https://github.com/bazelbuild/rules_docker/archive/refs/tags/v0.25.0.tar.gz"],
    ),
    io_bazel_rules_go = dict(
        sha256 = "099a9fb96a376ccbbb7d291ed4ecbdfd42f6bc822ab77ae6f1b5cb9e914e94fa",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.35.0/rules_go-v0.35.0.zip",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.35.0/rules_go-v0.35.0.zip",
        ],
    ),
    io_bazel_rules_k8s = dict(
        sha256 = "a08850199d6900328ef899906717fb1dfcc6cde62701c63725748b2e6ca1d5d9",
        strip_prefix = "rules_k8s-d05cbea5c56738ef02c667c10951294928a1d64a",
        urls = [
            "https://github.com/bazelbuild/rules_k8s/archive/d05cbea5c56738ef02c667c10951294928a1d64a.tar.gz",
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
        sha256 = "6eaf86ead73e23988fe192da1db68f4d3828bcdd0f3a9dc195935e339c95dbdc",
        strip_prefix = "tensorflow-2.9.1",
        urls = ["https://github.com/tensorflow/tensorflow/archive/refs/tags/v2.9.1.tar.gz"],
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
        urls = ["https://github.com/bazelbuild/rules_jvm_external/archive/refs/tags/4.2.tar.gz"],
        sha256 = "2cd77de091e5376afaf9cc391c15f093ebd0105192373b334f0a855d89092ad5",
        strip_prefix = "rules_jvm_external-4.2",
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
