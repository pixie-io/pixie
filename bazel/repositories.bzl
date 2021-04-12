load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(":repository_locations.bzl", "REPOSITORY_LOCATIONS")

# Make all contents of an external repository accessible under a filegroup.
# Used for external HTTP archives, e.g. cares.
BUILD_ALL_CONTENT = """filegroup(name = "all", srcs = glob(["**"]), visibility = ["//visibility:public"])"""

def _repo_impl(name, **kwargs):
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

# For bazel repos do not require customization.
def _bazel_repo(name, **kwargs):
    _repo_impl(name, **kwargs)

# With a predefined "include all files" BUILD file for a non-Bazel repo.
def _include_all_repo(name, **kwargs):
    kwargs["build_file_content"] = BUILD_ALL_CONTENT
    _repo_impl(name, **kwargs)

def _com_llvm_lib():
    native.new_local_repository(
        name = "com_llvm_lib",
        build_file = "third_party/llvm.BUILD",
        path = "/opt/clang-11.1",
    )

    native.new_local_repository(
        name = "com_llvm_lib_libcpp",
        build_file = "third_party/llvm.BUILD",
        path = "/opt/clang-11.1-libc++",
    )

def _com_github_threadstacks():
    native.local_repository(
        name = "com_github_threadstacks",
        path = "third_party/threadstacks",
    )

def _cc_deps():
    _bazel_repo("com_google_protobuf", patches = ["//third_party:protobuf.patch", "//third_party:protobuf_text_format.patch"], patch_args = ["-p1"])
    _bazel_repo("com_google_benchmark")
    _bazel_repo("com_google_googletest")
    _bazel_repo("com_github_gflags_gflags")
    _bazel_repo("com_github_google_glog")
    _bazel_repo("com_google_absl")
    _bazel_repo("com_google_flatbuffers")
    _bazel_repo("org_tensorflow")
    _bazel_repo("com_github_neargye_magic_enum")
    _bazel_repo("rules_python")

    _include_all_repo("com_github_gperftools_gperftools", patch_cmds = ["./autogen.sh"])
    _include_all_repo("com_github_nats_io_natsc", patches = ["//third_party:natsc.patch"], patch_args = ["-p1"])
    _include_all_repo("com_github_libuv_libuv", patches = ["//third_party:libuv.patch"], patch_args = ["-p1"])
    _include_all_repo("com_github_libarchive_libarchive")

    _repo_impl("com_google_double_conversion", build_file = "//third_party:double_conversion.BUILD")
    _repo_impl("com_github_rlyeh_sole", build_file = "//third_party:sole.BUILD")
    _repo_impl("com_github_tencent_rapidjson", build_file = "//third_party:rapidjson.BUILD")
    _repo_impl("com_github_ariafallah_csv_parser", build_file = "//third_party:csv_parser.BUILD")
    _repo_impl("com_github_cameron314_concurrentqueue", build_file = "//third_party:concurrentqueue.BUILD")
    _repo_impl("com_github_arun11299_cpp_jwt", build_file = "//third_party:cpp_jwt.BUILD")
    _repo_impl("com_github_cyan4973_xxhash", build_file = "//third_party:xxhash.BUILD")
    _repo_impl("com_github_nlohmann_json", build_file = "//third_party:nlohmann_json.BUILD")
    _repo_impl(
        "com_github_google_sentencepiece",
        build_file = "//third_party:sentencepiece.BUILD",
        patches = ["//third_party:sentencepiece.patch"],
        patch_args = ["-p1"],
    )

    _com_github_threadstacks()

def _go_deps():
    # Add go specific imports here when necessary.
    pass

def list_pl_deps(name):
    repo_urls = list()
    for repo_name, repo_config in REPOSITORY_LOCATIONS.items():
        urls = repo_config["urls"]
        best_url = None
        for url in urls:
            if url.startswith("https://github.com") or best_url == None:
                best_url = url
        repo_urls.append(best_url)

    native.genrule(
        name = name,
        outs = ["{}.out".format(name)],
        cmd = 'echo "{}" > $@'.format("\n".join(repo_urls)),
        visibility = ["//visibility:public"],
    )

def pl_deps():
    _com_llvm_lib()

    _bazel_repo("io_bazel_rules_go")
    _bazel_repo("com_github_bazelbuild_buildtools")
    _bazel_repo("bazel_skylib")

    _bazel_repo("io_bazel_toolchains")
    _bazel_repo("distroless")
    _bazel_repo("com_google_boringssl")
    _bazel_repo("rules_foreign_cc")
    _bazel_repo("io_bazel_rules_k8s")
    _bazel_repo("io_bazel_rules_closure")

    _repo_impl("io_bazel_rules_docker")
    _repo_impl("bazel_gazelle")
    _repo_impl("com_github_grpc_grpc", patches = ["//third_party:grpc.patch"], patch_args = ["-p1"])
    _repo_impl("com_intel_tbb", build_file = "@pl//third_party:tbb.BUILD")
    _repo_impl("com_google_farmhash", build_file = "@pl//third_party:farmhash.BUILD")
    _repo_impl("com_github_h2o_picohttpparser", build_file = "@pl//third_party:picohttpparser.BUILD")

    _cc_deps()
    _go_deps()
