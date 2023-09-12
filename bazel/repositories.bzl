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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/jdk:remote_java_repository.bzl", "remote_java_repository")
load("//bazel/cc_toolchains:deps.bzl", "cc_toolchain_config_repo")
load(":repository_locations.bzl", "GIT_REPOSITORY_LOCATIONS", "LOCAL_REPOSITORY_LOCATIONS", "REPOSITORY_LOCATIONS")

# Make all contents of an external repository accessible under a filegroup.
# Used for external HTTP archives, e.g. cares.
BUILD_ALL_CONTENT = """filegroup(name = "all", srcs = glob(["**"]), visibility = ["//visibility:public"])"""

def _http_archive_repo_impl(name, **kwargs):
    # `existing_rule_keys` contains the names of repositories that have already
    # been defined in the Bazel workspace. By skipping repos with existing keys,
    # users can override dependency versions by using standard Bazel repository
    # rules in their WORKSPACE files.
    existing_rule_keys = native.existing_rules().keys()
    if name in existing_rule_keys:
        # This repository has already been defined, probably because the user
        # wants to override the version. Do nothing.
        return

    location = REPOSITORY_LOCATIONS[name]

    # HTTP tarball at a given URL. Add a BUILD file if requested.
    http_archive(
        name = name,
        urls = location["urls"],
        sha256 = location["sha256"],
        strip_prefix = location.get("strip_prefix", ""),
        **kwargs
    )

def _git_repo_impl(name, **kwargs):
    # `existing_rule_keys` contains the names of repositories that have already
    # been defined in the Bazel workspace. By skipping repos with existing keys,
    # users can override dependency versions by using standard Bazel repository
    # rules in their WORKSPACE files.
    existing_rule_keys = native.existing_rules().keys()
    if name in existing_rule_keys:
        # This repository has already been defined, probably because the user
        # wants to override the version. Do nothing.
        return

    location = GIT_REPOSITORY_LOCATIONS[name]

    # HTTP tarball at a given URL. Add a BUILD file if requested.
    new_git_repository(
        name = name,
        remote = location["remote"],
        commit = location["commit"],
        init_submodules = True,
        recursive_init_submodules = True,
        shallow_since = location.get("shallow_since", ""),
        **kwargs
    )

def _local_repo_impl(name, **kwargs):
    # `existing_rule_keys` contains the names of repositories that have already
    # been defined in the Bazel workspace. By skipping repos with existing keys,
    # users can override dependency versions by using standard Bazel repository
    # rules in their WORKSPACE files.
    existing_rule_keys = native.existing_rules().keys()
    if name in existing_rule_keys:
        # This repository has already been defined, probably because the user
        # wants to override the version. Do nothing.
        return

    location = LOCAL_REPOSITORY_LOCATIONS[name]

    native.new_local_repository(
        name = name,
        path = location["path"],
        **kwargs
    )

def _git_repo(name, **kwargs):
    _git_repo_impl(name, **kwargs)

def _local_repo(name, **kwargs):  # buildifier: disable=unused-variable
    _local_repo_impl(name, **kwargs)

# For bazel repos do not require customization.
def _bazel_repo(name, **kwargs):
    _http_archive_repo_impl(name, **kwargs)

# With a predefined "include all files" BUILD file for a non-Bazel repo.
def _include_all_repo(name, **kwargs):
    kwargs["build_file_content"] = BUILD_ALL_CONTENT
    _http_archive_repo_impl(name, **kwargs)

def _com_llvm_lib():
    _bazel_repo("com_llvm_lib_x86_64_glibc_host", build_file = "//bazel/external:llvm.BUILD")
    _bazel_repo("com_llvm_lib_libcpp_x86_64_glibc_host", build_file = "//bazel/external:llvm.BUILD")
    _bazel_repo("com_llvm_lib_libcpp_x86_64_glibc_host_asan", build_file = "//bazel/external:llvm.BUILD")
    _bazel_repo("com_llvm_lib_libcpp_x86_64_glibc_host_msan", build_file = "//bazel/external:llvm.BUILD")
    _bazel_repo("com_llvm_lib_libcpp_x86_64_glibc_host_tsan", build_file = "//bazel/external:llvm.BUILD")

    _bazel_repo("com_llvm_lib_x86_64_glibc2_36", build_file = "//bazel/external:llvm.BUILD")
    _bazel_repo("com_llvm_lib_libcpp_x86_64_glibc2_36", build_file = "//bazel/external:llvm.BUILD")

    _bazel_repo("com_llvm_lib_aarch64_glibc2_36", build_file = "//bazel/external:llvm.BUILD")
    _bazel_repo("com_llvm_lib_libcpp_aarch64_glibc2_36", build_file = "//bazel/external:llvm.BUILD")

def _cc_deps():
    # Dependencies with native bazel build files.

    _bazel_repo("upb")
    _bazel_repo("com_google_protobuf", patches = ["//bazel/external:protobuf_gogo_hack.patch", "//bazel/external:protobuf_text_format.patch", "//bazel/external:protobuf_warning.patch"], patch_args = ["-p1"])
    _bazel_repo("com_github_grpc_grpc", patches = ["//bazel/external:grpc.patch", "//bazel/external:grpc_go_toolchain.patch", "//bazel/external:grpc_test_visibility.patch"], patch_args = ["-p1"])

    _bazel_repo("boringssl", patches = ["//bazel/external:boringssl.patch"], patch_args = ["-p0"])
    _bazel_repo("com_google_benchmark")
    _bazel_repo("com_google_googletest")
    _bazel_repo("com_github_gflags_gflags")
    _bazel_repo("com_github_google_glog")
    _bazel_repo("com_google_absl")
    _bazel_repo("com_google_flatbuffers")
    _bazel_repo("cpuinfo", patches = ["//bazel/external:cpuinfo.patch"], patch_args = ["-p1"])
    _bazel_repo("org_tensorflow", patches = ["//bazel/external:tensorflow_disable_llvm.patch", "//bazel/external:tensorflow_disable_mirrors.patch", "//bazel/external:tensorflow_disable_py.patch"], patch_args = ["-p1"])
    _bazel_repo("com_github_neargye_magic_enum")
    _bazel_repo("com_github_thoughtspot_threadstacks")
    _bazel_repo("com_googlesource_code_re2", patches = ["//bazel/external:re2_warning.patch"], patch_args = ["-p1"])
    _bazel_repo("com_intel_tbb")

    # Remove the pull and push directory since they depends on civet and we don't
    # want to pull in the dependency for now.
    _bazel_repo("com_github_jupp0r_prometheus_cpp", patch_cmds = ["rm -rf pull push 3rdparty"])

    # Dependencies where we provide an external BUILD file.
    _bazel_repo("com_github_apache_arrow", build_file = "//bazel/external:arrow.BUILD")
    _bazel_repo("com_github_ariafallah_csv_parser", build_file = "//bazel/external:csv_parser.BUILD")
    _bazel_repo("com_github_arun11299_cpp_jwt", build_file = "//bazel/external:cpp_jwt.BUILD")
    _bazel_repo("com_github_cameron314_concurrentqueue", build_file = "//bazel/external:concurrentqueue.BUILD")
    _bazel_repo("com_github_cyan4973_xxhash", build_file = "//bazel/external:xxhash.BUILD")
    _bazel_repo("com_github_nlohmann_json", build_file = "//bazel/external:nlohmann_json.BUILD")
    _bazel_repo("com_github_packetzero_dnsparser", build_file = "//bazel/external:dnsparser.BUILD")
    _bazel_repo("com_github_rlyeh_sole", patches = ["//bazel/external:sole.patch"], patch_args = ["-p1"], build_file = "//bazel/external:sole.BUILD")
    _bazel_repo("com_github_serge1_elfio", build_file = "//bazel/external:elfio.BUILD")
    _bazel_repo("com_github_derrickburns_tdigest", build_file = "//bazel/external:tdigest.BUILD")
    _bazel_repo("com_github_tencent_rapidjson", build_file = "//bazel/external:rapidjson.BUILD")
    _bazel_repo("com_github_vinzenz_libpypa", build_file = "//bazel/external:libpypa.BUILD")
    _bazel_repo("com_google_double_conversion", build_file = "//bazel/external:double_conversion.BUILD")
    _bazel_repo("com_github_google_sentencepiece", build_file = "//bazel/external:sentencepiece.BUILD", patches = ["//bazel/external:sentencepiece.patch"], patch_args = ["-p1"])
    _bazel_repo("com_github_antlr_antlr4", build_file = "//bazel/external:antlr4.BUILD")
    _bazel_repo("com_github_antlr_grammars_v4", build_file = "//bazel/external:antlr_grammars.BUILD", patches = ["//bazel/external:antlr_grammars.patch"], patch_args = ["-p1"])
    _bazel_repo("com_github_pgcodekeeper_pgcodekeeper", build_file = "//bazel/external:pgsql_grammar.BUILD", patches = ["//bazel/external:pgsql_grammar.patch"], patch_args = ["-p1"])
    _bazel_repo("com_github_simdutf_simdutf", build_file = "//bazel/external:simdutf.BUILD")
    _bazel_repo("com_github_USCiLab_cereal", build_file = "//bazel/external:cereal.BUILD")
    _bazel_repo("com_google_farmhash", build_file = "//bazel/external:farmhash.BUILD")
    _bazel_repo("com_github_h2o_picohttpparser", build_file = "//bazel/external:picohttpparser.BUILD")
    _bazel_repo("com_github_opentelemetry_proto", build_file = "//bazel/external:opentelemetry.BUILD")
    _bazel_repo("com_github_uriparser_uriparser", build_file = "//bazel/external:uriparser.BUILD")
    _bazel_repo("com_github_libbpf_libbpf", build_file = "//bazel/external:libbpf.BUILD")
    _bazel_repo("com_github_mongodb_mongo_c_driver", build_file = "//bazel/external:mongo_c_driver.BUILD")

    # Uncomment these to develop bcc and/or bpftrace locally. Should also comment out the corresponding _bazel_repo lines.
    # _local_repo("com_github_iovisor_bcc", build_file = "//bazel/external/local_dev:bcc.BUILD")
    # _local_repo("com_github_iovisor_bpftrace", build_file = "//bazel/external/local_dev:bpftrace.BUILD")
    _bazel_repo("com_github_iovisor_bcc", build_file = "//bazel/external:bcc.BUILD")
    _bazel_repo("com_github_iovisor_bpftrace", build_file = "//bazel/external:bpftrace.BUILD")

    # TODO(jps): For jattach, consider using a patch and directly pulling from upstream (vs. fork).
    _git_repo("com_github_apangin_jattach", build_file = "//bazel/external:jattach.BUILD")

    # Dependencies used in foreign cc rules (e.g. cmake-based builds)
    _include_all_repo("com_github_gperftools_gperftools")
    _include_all_repo("com_github_nats_io_natsc")
    _include_all_repo("com_github_libuv_libuv", patches = ["//bazel/external:libuv.patch"], patch_args = ["-p1"])
    _include_all_repo("com_github_libarchive_libarchive", patches = ["//bazel/external:libarchive.patch"], patch_args = ["-p1"])

    _bazel_repo("org_libc_musl", build_file = "//bazel/external:musl.BUILD")

def _java_deps():
    _bazel_repo("com_oracle_openjdk_18", build_file = "//bazel/external:jdk_includes.BUILD")
    remote_java_repository(
        name = "remotejdk_openjdk_graal_17",
        version = "17",
        prefix = "remotejdk_openjdk_graal",
        target_compatible_with = [
            "@platforms//os:linux",
        ],
        sha256 = "102db28b450ff5eb8c497aacaececc5263a4e50e64b7cdc5c7baa8b216e73531",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/graalvm%2Fpl1/graalvm-native-image-22.3.0-pl1.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/graalvm-native-image-22.3.0-pl1.tar.gz",
        ],
    )

def _list_pl_deps(name):
    modules = dict()
    for _, repo_config in REPOSITORY_LOCATIONS.items():
        if "manual_license_name" in repo_config:
            modules["#manual-license-name:" + repo_config["manual_license_name"]] = True
            continue
        urls = repo_config["urls"]
        best_url = None
        for url in urls:
            if url.startswith("https://github.com") or best_url == None:
                best_url = url
        modules[best_url] = True

    for _, repo_config in GIT_REPOSITORY_LOCATIONS.items():
        remote = repo_config["remote"]
        if remote.endswith(".git"):
            remote = remote[:-len(".git")]
        if repo_config["commit"]:
            remote = remote + "/commit/" + repo_config["commit"]
        modules[remote] = True

    module_lines = []
    for key in modules.keys():
        module_lines.append(key)

    native.genrule(
        name = name,
        outs = ["{}.out".format(name)],
        cmd = 'echo "{}" > $@'.format("\n".join(module_lines)),
        visibility = ["//visibility:public"],
    )

def _pl_cc_toolchain_deps():
    _bazel_repo("bazel_skylib")
    cc_toolchain_config_repo("unix_cc_toolchain_config", patch = "//bazel/cc_toolchains:unix_cc_toolchain_config.patch")

def _pl_deps():
    _bazel_repo("bazel_gazelle")
    _bazel_repo("io_bazel_rules_go", patches = ["//bazel/external:rules_go.patch"], patch_args = ["-p1"])
    _bazel_repo("io_bazel_rules_scala")
    _bazel_repo("rules_jvm_external")
    _bazel_repo("rules_foreign_cc")
    _bazel_repo("io_bazel_rules_k8s")
    _bazel_repo("io_bazel_rules_closure")
    _bazel_repo("io_bazel_rules_docker", patches = ["//bazel/external:rules_docker.patch", "//bazel/external:rules_docker_arch.patch"], patch_args = ["-p1"])
    _bazel_repo("rules_python")
    _bazel_repo("rules_pkg")
    _bazel_repo("com_github_bazelbuild_buildtools")
    _bazel_repo("com_github_fmeum_rules_meta")
    _bazel_repo("com_google_protobuf_javascript", patches = ["//bazel/external:protobuf_javascript.patch"], patch_args = ["-p1"])

    _com_llvm_lib()
    _cc_deps()

    _java_deps()

list_pl_deps = _list_pl_deps
pl_deps = _pl_deps
pl_cc_toolchain_deps = _pl_cc_toolchain_deps
