workspace(name = "px")

load("//:workspace.bzl", "check_min_bazel_version")

check_min_bazel_version("4.0.0")

load("//bazel:repositories.bzl", "pl_deps")

# Install Pixie Labs Dependencies.
pl_deps()

# Order is important. Try to go from most basic/primitive to higher level packages.
# - protobuf_deps
# - grpc_deps (must come after protobuf_deps)
# - go_rules_dependencies
# - apple_rules_dependencies (must come after grpc_deps)
# ...
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(go_version = "1.16")

# These dependencies are needed by GRPC.
load("@build_bazel_rules_apple//apple:repositories.bzl", "apple_rules_dependencies")

apple_rules_dependencies()

load("@build_bazel_apple_support//lib:repositories.bzl", "apple_support_dependencies")

apple_support_dependencies()

load("//bazel:pl_workspace.bzl", "pl_container_images", "pl_workspace_setup")

pl_workspace_setup()

# The pip_deps rule cannot be loaded until we load all the basic packages in the Pixie
# workspace. Also, bazel requires that loads are done at the top level (not in a function), so
# we need to pull it out over here.
load("@io_bazel_rules_docker//repositories:py_repositories.bzl", "py_deps")

py_deps()

# The docker images can't be loaded until all pip_deps are satisfied.
pl_container_images()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

load("//bazel:gogo.bzl", "gogo_grpc_proto")

gogo_grpc_proto(name = "gogo_grpc_proto")

# Setup tensorflow.
load("@org_tensorflow//tensorflow:workspace.bzl", "tf_repositories")

tf_repositories()

load("//bazel:pl_workspace.bzl", "pl_model_files")

pl_model_files()

# Setup the environment for the open-source python API.
load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "vizier_api_python_deps",
    requirements_lock = "//src/api/python:requirements.txt",
)

load("@vizier_api_python_deps//:requirements.bzl", "install_deps")

install_deps()

# Setup thrift.
load("@rules_jvm_external//:defs.bzl", "maven_install")

finagle_version = "21.4.0"
scala_version = "2.13.6"
scala_minor_version = ".".join(scala_version.split(".")[:2])

maven_install(
    artifacts = [
        "com.twitter:finagle-thriftmux_%s:%s" % (scala_minor_version, finagle_version),
        "com.twitter:finagle-mux_%s:%s" % (scala_minor_version, finagle_version),
        "com.twitter:finagle-core_%s:%s" % (scala_minor_version, finagle_version),
        "com.twitter:scrooge-core_%s:%s" % (scala_minor_version, finagle_version),
        "com.twitter:finagle-http_%s:%s" % (scala_minor_version, finagle_version),
        "org.apache.thrift:libthrift:0.10.0",
    ],
    repositories = ["https://repo1.maven.org/maven2"],
)

# Setup scala toolchains.
load("@io_bazel_rules_scala//:scala_config.bzl", "scala_config")

scala_config(scala_version = scala_version)

load("@io_bazel_rules_scala//scala:scala.bzl", "scala_repositories")

scala_repositories()

load("@io_bazel_rules_scala//scala:toolchains.bzl", "scala_register_toolchains")

scala_register_toolchains()

load(
    "@io_bazel_rules_docker//scala:image.bzl",
    scala_image_repos = "repositories",
)

scala_image_repos()

load("@io_bazel_rules_scala//twitter_scrooge:twitter_scrooge.bzl", "twitter_scrooge")

twitter_scrooge()

# twitter_scrooge will use incompatible versions of @scrooge_jars and @thrift_jars.
# These bind statements ensure that the correct versions of finagle libthrift are used
# so that compiliation is successful. See https://github.com/bazelbuild/rules_scala/issues/592
# and https://github.com/bazelbuild/rules_scala/pull/847 for more details.
bind(
    name = "io_bazel_rules_scala/dependency/thrift/scrooge_core",
    actual = "//src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux:scrooge_jars",
)

bind(
    name = "io_bazel_rules_scala/dependency/thrift/libthrift",
    actual = "//src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux:thrift_jars",
)

# gazelle:repo bazel_gazelle

load("//:go_deps.bzl", "pl_go_dependencies")

# gazelle:repository_macro go_deps.bzl%pl_go_dependencies
pl_go_dependencies()
