workspace(name = "px")

load("//:workspace.bzl", "check_min_bazel_version")

check_min_bazel_version("6.0.0")

load("//bazel:repositories.bzl", "pl_cc_toolchain_deps", "pl_deps")

pl_cc_toolchain_deps()

load("//bazel/cc_toolchains/sysroots:sysroots.bzl", "pl_sysroot_deps")

pl_sysroot_deps()

load("//bazel/cc_toolchains:toolchains.bzl", "pl_register_cc_toolchains")

pl_register_cc_toolchains()

# Install Pixie Labs Dependencies.
pl_deps()

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
    version = "1.21.0",
)

go_rules_dependencies()

go_register_toolchains()

# Pixie go dependencies need to be loaded before other go dependencies
# to make sure we get the correct version.
# gazelle:repository_macro go_deps.bzl%pl_go_dependencies
pl_go_dependencies()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@io_bazel_rules_scala//:scala_config.bzl", "scala_config")

scala_version = "2.13.6"

scala_config(scala_version = scala_version)

load("@io_bazel_rules_scala//scala:scala.bzl", "scala_repositories")

scala_repositories()

load("@io_bazel_rules_scala//scala:toolchains.bzl", "scala_register_toolchains")

scala_register_toolchains()

# These dependencies are needed by GRPC.
load("@build_bazel_rules_apple//apple:repositories.bzl", "apple_rules_dependencies")

apple_rules_dependencies()

load("@build_bazel_apple_support//lib:repositories.bzl", "apple_support_dependencies")

apple_support_dependencies()

load("//bazel:pl_workspace.bzl", "pl_container_images", "pl_model_files", "pl_workspace_setup")

pl_workspace_setup()

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

load("@rules_python//python:repositories.bzl", "python_register_toolchains")

python_register_toolchains(
    name = "python3_10",
    # Available versions are listed in @rules_python//python:versions.bzl.
    # We recommend using the same version your team is already standardized on.
    python_version = "3.10",
)

load("@python3_10//:defs.bzl", "interpreter")

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
    requirements_lock = "//src/api/python/doc:requirements.bazel.txt",
)

load("@pxapi_python_doc_deps//:requirements.bzl", pxapi_py_doc_install_deps = "install_deps")

pxapi_py_doc_install_deps()

# Setup thrift: used for building Stirling tracing targets.
load("//bazel:thrift.bzl", "thrift_deps")

thrift_deps(scala_version = scala_version)

load("@thrift_deps//:defs.bzl", thrift_pinned_maven_install = "pinned_maven_install")

thrift_pinned_maven_install()

# twitter_scrooge will use incompatible versions of @scrooge_jars and @thrift_jars.
# These bind statements ensure that the correct versions of finagle libthrift, scrooge core
# and scrooge generator are used to ensure successful compilation.
# See https://github.com/bazelbuild/rules_scala/issues/592 and
# https://github.com/bazelbuild/rules_scala/pull/847 for more details.
bind(
    name = "io_bazel_rules_scala/dependency/thrift/scrooge_core",
    actual = "//src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux:scrooge_jars",
)

bind(
    name = "io_bazel_rules_scala/dependency/thrift/scrooge_generator",
    actual = "//src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux:scrooge_jars",
)

bind(
    name = "io_bazel_rules_scala/dependency/thrift/libthrift",
    actual = "//src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux:thrift_jars",
)

# gazelle:repo bazel_gazelle
# Gazelle depes need to be loaded last to make sure they don't override our dependencies.
# The first one wins when it comes to package declaration.
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

gazelle_dependencies(go_sdk = "go_sdk")

# Download alternative go toolchains after all other dependencies, so that they aren't used by external dependencies.
go_download_sdk(
    name = "go_sdk_1_16",
    version = "1.16.15",
)

go_download_sdk(
    name = "go_sdk_1_17",
    version = "1.17.13",
)

go_download_sdk(
    name = "go_sdk_1_18",
    version = "1.18.10",
)

go_download_sdk(
    name = "go_sdk_1_19",
    version = "1.19.10",
)

go_download_sdk(
    name = "go_sdk_1_20",
    version = "1.20.5",
)

go_download_sdk(
    name = "go_sdk_1_21",
    version = "1.21.0",
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
    # TODO(james): update this to 1.21.0, once there is a 1.21.1 release.
    version = "1.20.4",
)

pip_parse(
    name = "amqp_gen_reqs",
    requirements_lock = "//src/stirling/source_connectors/socket_tracer/protocols/amqp/amqp_code_generator:requirements.bazel.txt",
)

load("@amqp_gen_reqs//:requirements.bzl", amp_gen_install_deps = "install_deps")

amp_gen_install_deps()

pip_parse(
    name = "protocol_inference",
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
    requirements_lock = "//src/stirling/source_connectors/socket_tracer/testing/containers/mongodb:requirements.bazel.txt",
)

load("@mongodb_bpf_test_requirements//:requirements.bzl", mongodb_bpf_test_install_deps = "install_deps")

mongodb_bpf_test_install_deps()
