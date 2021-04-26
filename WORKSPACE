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

load("//bazel:pl_workspace.bzl", "pl_docker_images", "pl_workspace_setup")

pl_workspace_setup()

# The pip_deps rule cannot be loaded until we load all the basic packages in the Pixie
# workspace. Also, bazel requires that loads are done at the top level (not in a function), so
# we need to pull it out over here.
load("@io_bazel_rules_docker//repositories:py_repositories.bzl", "py_deps")

py_deps()

# The docker images can't be loaded until all pip_deps are satisfied.
pl_docker_images()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

load("//bazel:gogo.bzl", "gogo_grpc_proto")

gogo_grpc_proto(name = "gogo_grpc_proto")

# Setup tensorflow.
load("@org_tensorflow//tensorflow:workspace.bzl", "tf_repositories")

tf_repositories()

load("@io_bazel_toolchains//rules:gcs.bzl", "gcs_file")

# Download model files
gcs_file(
    name = "embedding_model",
    bucket = "gs://pixie-dev-public",
    downloaded_file_path = "embedding.proto",
    file = "ml-data/models/current-embedding-model.proto",
    sha256 = "a23c515c139670e71c0cad5c962f7e2d968fcc57ab251e49f4b5636134628813",
)

gcs_file(
    name = "sentencepiece_model",
    bucket = "gs://pixie-dev-public",
    downloaded_file_path = "sentencepiece.proto",
    file = "ml-data/models/current-sentencepiece-model.proto",
    sha256 = "7e17e04ecc207d9204dc8755357f988bf77c135f7a34a88984943c8649d6a790",
)

# Setup the environment for the open-source python API.
load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "vizier_api_python_deps",
    requirements_lock = "//src/api/python:requirements.txt",
)

load("@vizier_api_python_deps//:requirements.bzl", "install_deps")

install_deps()

# gazelle:repo bazel_gazelle

load("//:go_deps.bzl", "pl_go_dependencies")

# gazelle:repository_macro go_deps.bzl%pl_go_dependencies
pl_go_dependencies()
