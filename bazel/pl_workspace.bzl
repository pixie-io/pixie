load("@io_bazel_rules_go//go:def.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
load("@com_github_bazelbuild_buildtools//buildifier:deps.bzl", "buildifier_dependencies")
load("@com_github_grpc_grpc//:bazel/grpc_deps.bzl", "grpc_deps")
load("@io_bazel_rules_docker//go:image.bzl", _go_image_repos = "repositories")
load("@io_bazel_rules_docker//cc:image.bzl", _cc_image_repos = "repositories")
load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_pull",
    container_repositories = "repositories",
)

def _go_setup():
    go_rules_dependencies()
    go_register_toolchains()
    gazelle_dependencies()

def _docker_setup():
    _go_image_repos()
    _cc_image_repos()

    # Import NGINX repo.
    container_pull(
        name = "nginx_base",
        digest = "sha256:9ad0746d8f2ea6df3a17ba89eca40b48c47066dfab55a75e08e2b70fc80d929e",
        registry = "index.docker.io",
        repository = "library/nginx",
    )

def pl_workspace_setup():
    _go_setup()
    buildifier_dependencies()
    grpc_deps()
    _docker_setup()
