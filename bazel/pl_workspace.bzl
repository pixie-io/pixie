load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
load("@com_github_bazelbuild_buildtools//buildifier:deps.bzl", "buildifier_dependencies")
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
load("@io_bazel_rules_docker//go:image.bzl", _go_image_repos = "repositories")
load("@io_bazel_rules_docker//cc:image.bzl", _cc_image_repos = "repositories")
load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_pull",
)
load("@distroless//package_manager:package_manager.bzl", "package_manager_repositories")
load("@distroless//package_manager:dpkg.bzl", "dpkg_list", "dpkg_src")

def _go_setup():
    go_rules_dependencies()
    go_register_toolchains()
    gazelle_dependencies()

# Sets up package manager which we use build deploy images.
def _package_manager_setup():
    package_manager_repositories()

    dpkg_src(
        name = "debian_stretch",
        arch = "amd64",
        distro = "stretch",
        sha256 = "9aea0e4c9ce210991c6edcb5370cb9b11e9e554a0f563e7754a4028a8fd0cb73",
        snapshot = "20171101T160520Z",
        url = "http://snapshot.debian.org/archive",
    )

    dpkg_list(
        name = "package_bundle",
        packages = [
            "libc6",
            "libelf1",
            "liblzma5",
            "libtinfo5",
            "libunwind8",
            "zlib1g",
        ],
        sources = ["@debian_stretch//file:Packages.json"],
    )

def _docker_setup():
    _go_image_repos()
    _cc_image_repos()
    _package_manager_setup()

    # Import NGINX repo.
    container_pull(
        name = "nginx_base",
        digest = "sha256:9ad0746d8f2ea6df3a17ba89eca40b48c47066dfab55a75e08e2b70fc80d929e",
        registry = "index.docker.io",
        repository = "library/nginx",
    )

    # Import CC base image.
    container_pull(
        name = "cc_base",
        # From : March 27, 2019
        digest = "sha256:482e7efb3245ded60e9ced05909551fc14d39b47e2cc643830f4466010c25372",
        registry = "gcr.io",
        repository = "distroless/cc",
    )

    # Import CC base debug image.
    container_pull(
        name = "cc_base_debug",
        # From : April 22, 2019
        digest = "sha256:8bd401c66e7bf2432a8f22052060021ceb485d00b78e916149a5b3738f24c787",
        registry = "gcr.io",
        repository = "distroless/cc",
    )

    # Import dev image.
    # Warning: Make sure to update the digest if you need an up-to-date version.
    container_pull(
        name = "dev_image",
        digest = "sha256:e0f7ba46700d4eb0b823e4949cea3219e408f2443fc5612b22785be0ded46b96",
        registry = "gcr.io",
        repository = "pl-dev-infra/dev_image",
    )

def pl_workspace_setup():
    _go_setup()
    buildifier_dependencies()
    grpc_deps()
    _docker_setup()
