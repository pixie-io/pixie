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

def _cc_deps():
    _repository_impl(name = "com_google_benchmark")
    _repository_impl(
        name = "com_google_googletest",
    )
    _repository_impl(name = "com_github_gflags_gflags")
    _repository_impl(name = "com_github_google_glog")
    _repository_impl(name = "com_google_absl")
    _com_google_double_conversion()

def _go_deps():
    # Add go specific imports here when necessary.
    pass

def pl_deps():
    _com_iovisor_bcc()
    _com_llvm_lib()

    _repository_impl(name = "bazel_gazelle")
    _repository_impl(name = "com_github_bazelbuild_buildtools")
    _repository_impl(name = "io_bazel_rules_docker")
    _repository_impl(name = "io_bazel_rules_go")
    _repository_impl(name = "com_github_grpc_grpc")
    _repository_impl(name = "com_intel_tbb", build_file = "@pl//third_party:tbb.BUILD")
    _cc_deps()
    _go_deps()
