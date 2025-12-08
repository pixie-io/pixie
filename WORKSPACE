workspace(name = "px")

load("//:workspace.bzl", "check_min_bazel_version")

check_min_bazel_version("6.2.0")

load("//bazel:repositories.bzl", "pl_cc_toolchain_deps", "pl_deps")

pl_cc_toolchain_deps()

load("//bazel/cc_toolchains/sysroots:sysroots.bzl", "pl_sysroot_deps")

pl_sysroot_deps()

load("//bazel/cc_toolchains:toolchains.bzl", "pl_register_cc_toolchains")

pl_register_cc_toolchains()

# Install Pixie Labs Dependencies.
pl_deps()

load("@bazel_features//:deps.bzl", "bazel_features_deps")

bazel_features_deps()

# Order is important. Try to go from most basic/primitive to higher level packages.
# - go_rules_dependencies
# - protobuf_deps
# - grpc_deps (must come after protobuf_deps)
# - apple_rules_dependencies (must come after grpc_deps)
# ...
load("@io_bazel_rules_go//go:deps.bzl", "go_download_sdk", "go_register_toolchains", "go_rules_dependencies")
load("//:go_deps.bzl", "pl_go_dependencies", "pl_go_overrides")

# We need to override some of the go dependencies used by go_rules.
pl_go_overrides()

go_download_sdk(
    name = "go_sdk",
    version = "1.24.6",
)

go_rules_dependencies()

go_register_toolchains()

# Pixie go dependencies need to be loaded before other go dependencies
# to make sure we get the correct version.
# gazelle:repository_macro go_deps.bzl%pl_go_dependencies
pl_go_dependencies()

load("@rules_java//java:rules_java_deps.bzl", "rules_java_dependencies")

rules_java_dependencies()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@io_bazel_rules_scala//scala:deps.bzl", "rules_scala_dependencies")

rules_scala_dependencies()

load("@io_bazel_rules_scala//:scala_config.bzl", "scala_config")

scala_version = "2.13.16"

scala_config(scala_version = scala_version)

load("@io_bazel_rules_scala//scala:toolchains.bzl", "scala_register_toolchains", "scala_toolchains")

# Configure twitter_scrooge toolchain with custom dependencies from thrift_deps.
# Note: thrift_deps must be loaded before this is evaluated.
# The finagle dependencies (finagle-thrift, finagle-core) are needed as compile
# dependencies for the generated scrooge code when thrift services are defined.
# We use scrooge_core_with_finagle which bundles finagle deps with scrooge_core.
scala_toolchains(
    fetch_sources = True,
    twitter_scrooge = {
        "libthrift": "@thrift_deps//:org_apache_thrift_libthrift",
        # Use scrooge_core_with_finagle to include finagle on the compile classpath
        # for generated thrift service code. Must use @px// prefix to reference
        # the main workspace from within the generated @rules_scala_toolchains repo.
        "scrooge_core": "@px//src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux:scrooge_core_with_finagle",
        "scrooge_generator": "@thrift_deps//:com_twitter_scrooge_generator_2_13",
        "util_core": "@thrift_deps//:com_twitter_util_core_2_13",
        "util_logging": "@thrift_deps//:com_twitter_util_logging_2_13",
        "javax_annotation_api": "@thrift_deps//:javax_annotation_javax_annotation_api",
        "mustache": "@thrift_deps//:com_github_spullara_mustache_java_compiler",
        "scopt": "@thrift_deps//:com_github_scopt_scopt_2_13",
    },
)

scala_register_toolchains()

# These dependencies are needed by GRPC.
load("@build_bazel_rules_apple//apple:repositories.bzl", "apple_rules_dependencies")

apple_rules_dependencies()

load("@build_bazel_apple_support//lib:repositories.bzl", "apple_support_dependencies")

apple_support_dependencies()

load("//bazel:pl_workspace.bzl", "pl_container_images", "pl_model_files", "pl_workspace_setup")

pl_workspace_setup()

load("@rules_python//python:repositories.bzl", "py_repositories", "python_register_toolchains")

py_repositories()

# The pip_deps rule cannot be loaded until we load all the basic packages in the Pixie
# workspace. Also, bazel requires that loads are done at the top level (not in a function), so
# we need to pull it out over here.
load("@io_bazel_rules_docker//repositories:py_repositories.bzl", "py_deps")

py_deps()

load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "ubuntu_package_deps",
    requirements_lock = "//bazel/external/ubuntu_packages:requirements.bazel.txt",
)

load("@ubuntu_package_deps//:requirements.bzl", ubuntu_packages_install_deps = "install_deps")

ubuntu_packages_install_deps()

load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

rules_pkg_dependencies()

# The docker images can't be loaded until all pip_deps are satisfied.
pl_container_images()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

load("//bazel:gogo.bzl", "gogo_grpc_proto")

gogo_grpc_proto(name = "gogo_grpc_proto")

# Setup tensorflow.
load("@org_tensorflow//tensorflow:workspace3.bzl", "tf_workspace3")

tf_workspace3()

load("@org_tensorflow//tensorflow:workspace2.bzl", "tf_workspace2")

tf_workspace2()

load("@org_tensorflow//tensorflow:workspace1.bzl", "tf_workspace1")

tf_workspace1(with_rules_cc = False)

load("@org_tensorflow//tensorflow:workspace0.bzl", "tf_workspace0")

tf_workspace0()

pl_model_files()

python_register_toolchains(
    name = "python3_12",
    # Allow the root user to build the code base since this is a current requirement for
    # building in a containerized environment. See https://github.com/bazelbuild/rules_python/pull/713
    # for more details.
    ignore_root_user_error = True,
    # Available versions are listed in @rules_python//python:versions.bzl.
    # We recommend using the same version your team is already standardized on.
    python_version = "3.12",
)

load("@python3_12//:defs.bzl", "interpreter")

# Setup the environment for the open-source python API.
pip_parse(
    name = "vizier_api_python_deps",
    python_interpreter_target = interpreter,
    requirements_lock = "//src/api/python:requirements.bazel.txt",
)

load("@vizier_api_python_deps//:requirements.bzl", vizier_api_install_deps = "install_deps")

vizier_api_install_deps()

pip_parse(
    name = "pxapi_python_doc_deps",
    python_interpreter_target = interpreter,
    requirements_lock = "//src/api/python/doc:requirements.bazel.txt",
)

load("@pxapi_python_doc_deps//:requirements.bzl", pxapi_py_doc_install_deps = "install_deps")

pxapi_py_doc_install_deps()

# Setup thrift: used for building Stirling tracing targets.
load("//bazel:thrift.bzl", "thrift_deps")

thrift_deps(scala_version = scala_version)

load("@thrift_deps//:defs.bzl", thrift_pinned_maven_install = "pinned_maven_install")

thrift_pinned_maven_install()

# gazelle:repo bazel_gazelle
# Gazelle depes need to be loaded last to make sure they don't override our dependencies.
# The first one wins when it comes to package declaration.
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

gazelle_dependencies(go_sdk = "go_sdk")

go_download_sdk(
    name = "go_sdk_1_23",
    version = "1.23.12",
)

# The go_sdk_boringcrypto SDK is used for testing boringcrypto specific functionality (TLS tracing).
# This SDK is used for specific test cases and is not meant to be used wholesale for a particular go
# version.
#
# rules_go doesn't support using multiple SDKs with the same version and differing
# GOEXPERIMENTs. This can use the same version as our latest go version once
# https://github.com/bazelbuild/rules_go/issues/3582 is addressed.
go_download_sdk(
    name = "go_sdk_boringcrypto",
    experiments = ["boringcrypto"],
    version = "1.23.11",
)

pip_parse(
    name = "amqp_gen_reqs",
    python_interpreter_target = interpreter,
    requirements_lock = "//src/stirling/source_connectors/socket_tracer/protocols/amqp/amqp_code_generator:requirements.bazel.txt",
)

load("@amqp_gen_reqs//:requirements.bzl", amp_gen_install_deps = "install_deps")

amp_gen_install_deps()

pip_parse(
    name = "protocol_inference",
    python_interpreter_target = interpreter,
    requirements_lock = "//src/stirling/protocol_inference:requirements.bazel.txt",
)

load("@protocol_inference//:requirements.bzl", protocol_inference_install_deps = "install_deps")

protocol_inference_install_deps()

load(
    "@io_bazel_rules_docker//python3:image.bzl",
    py_image_repos = "repositories",
)

py_image_repos()

pip_parse(
    name = "amqp_bpf_test_requirements",
    python_interpreter_target = interpreter,
    requirements_lock = "//src/stirling/source_connectors/socket_tracer/testing/containers/amqp:requirements.bazel.txt",
)

load("@amqp_bpf_test_requirements//:requirements.bzl", ampq_bpf_test_install_deps = "install_deps")

ampq_bpf_test_install_deps()

load("@rules_jvm_external//:defs.bzl", "maven_install")

maven_install(
    name = "px_deps",
    artifacts = [
        "org.antlr:antlr4:4.11.1",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load("@px_deps//:defs.bzl", px_deps_pinned_maven_install = "pinned_maven_install")

px_deps_pinned_maven_install()

pip_parse(
    name = "mongodb_bpf_test_requirements",
    python_interpreter_target = interpreter,
    requirements_lock = "//src/stirling/source_connectors/socket_tracer/testing/containers/mongodb:requirements.bazel.txt",
)

load("@mongodb_bpf_test_requirements//:requirements.bzl", mongodb_bpf_test_install_deps = "install_deps")

mongodb_bpf_test_install_deps()
