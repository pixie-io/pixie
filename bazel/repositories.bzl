load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(":repository_locations.bzl", "REPOSITORY_LOCATIONS")

# Borrowed from Envoy (46c0693)
def _repository_impl(name, **kwargs):
    # `existing_rule_keys` contains the names of repositories that have already
    # been defined in the Bazel workspace. By skipping repos with existing keys,
    # users can override dependency versions by using standard Bazel repository
    # rules in their WORKSPACE files.
    existing_rule_keys = native.existing_rules().keys()
    if name in existing_rule_keys:
        # This repository has already been defined, probably because the user
        # wants to override the version. Do nothing.
        return

    loc_key = kwargs.pop("repository_key", name)
    location = REPOSITORY_LOCATIONS[loc_key]

    # HTTP tarball at a given URL. Add a BUILD file if requested.
    http_archive(
        name = name,
        urls = location["urls"],
        sha256 = location["sha256"],
        strip_prefix = location.get("strip_prefix", ""),
        **kwargs
    )

def _com_google_double_conversion():
    name = "com_google_double_conversion"
    location = REPOSITORY_LOCATIONS[name]
    http_archive(
        name = name,
        urls = location["urls"],
        sha256 = location["sha256"],
        strip_prefix = location.get("strip_prefix", ""),
        build_file = "//third_party:double_conversion.BUILD",
    )

def _com_llvm_lib():
    native.new_local_repository(
        name = "com_llvm_lib",
        build_file = "third_party/llvm.BUILD",
        path = "/opt/clang-7.0",
    )

def _com_iovisor_bcc():
    native.new_local_repository(
        name = "com_iovisor_bcc",
        build_file = "third_party/bcc.BUILD",
        path = "/opt/bcc",
    )

def _com_github_rlyeh_sole():
    name = "com_github_rlyeh_sole"
    location = REPOSITORY_LOCATIONS[name]
    http_archive(
        name = name,
        urls = location["urls"],
        sha256 = location["sha256"],
        strip_prefix = location.get("strip_prefix", ""),
        build_file = "//third_party:sole.BUILD",
    )

def _com_github_cpp_taskflow():
    name = "com_github_cpp_taskflow"
    location = REPOSITORY_LOCATIONS[name]
    http_archive(
        name = name,
        urls = location["urls"],
        sha256 = location["sha256"],
        strip_prefix = location.get("strip_prefix", ""),
        build_file = "//third_party:cpp_taskflow.BUILD",
    )

def _com_github_google_glog():
    name = "com_github_google_glog"
    location = REPOSITORY_LOCATIONS[name]
    http_archive(
        name = name,
        urls = location["urls"],
        sha256 = location["sha256"],
        strip_prefix = location.get("strip_prefix", ""),
        # TODO(zasgar): We can consider removing the stack trace patch when this lands:
        # https://github.com/google/glog/pull/347
        patches = ["//third_party:glog_stacktrace.patch"],
    )

def _com_github_tencent_rapidjson():
    name = "com_github_tencent_rapidjson"
    location = REPOSITORY_LOCATIONS[name]
    http_archive(
        name = name,
        urls = location["urls"],
        sha256 = location["sha256"],
        strip_prefix = location.get("strip_prefix", ""),
        build_file = "//third_party:rapidjson.BUILD",
    )

def _cc_deps():
    _repository_impl(name = "com_google_benchmark")
    _repository_impl(
        name = "com_google_googletest",
    )
    _repository_impl(name = "com_github_gflags_gflags")
    _com_github_google_glog()
    _repository_impl(name = "com_google_absl")
    _repository_impl(name = "com_google_flatbuffers")
    _com_github_rlyeh_sole()
    _com_google_double_conversion()
    _com_github_cpp_taskflow()
    _com_github_tencent_rapidjson()

def _go_deps():
    # Add go specific imports here when necessary.
    pass

def pl_deps():
    _com_iovisor_bcc()
    _com_llvm_lib()

    _repository_impl(name = "bazel_gazelle")
    _repository_impl(name = "com_github_bazelbuild_buildtools")
    _repository_impl(name = "bazel_skylib", repository_key = "io_bazel_rules_skylib")
    _repository_impl(name = "io_bazel_rules_docker")
    _repository_impl(name = "io_bazel_rules_go")
    _repository_impl(name = "com_github_grpc_grpc")
    _repository_impl(name = "com_intel_tbb", build_file = "@pl//third_party:tbb.BUILD")
    _repository_impl(name = "com_efficient_libcuckoo", build_file = "@pl//third_party:libcuckoo.BUILD")
    _repository_impl(name = "com_google_farmhash", build_file = "@pl//third_party:farmhash.BUILD")

    _cc_deps()
    _go_deps()
