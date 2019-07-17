load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
load("@com_github_bazelbuild_buildtools//buildifier:deps.bzl", "buildifier_dependencies")
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
load("@io_bazel_rules_docker//go:image.bzl", _go_image_repos = "repositories")
load("@io_bazel_rules_docker//cc:image.bzl", _cc_image_repos = "repositories")
load("@io_bazel_rules_docker//container:container.bzl", "container_pull")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
load("@distroless//package_manager:package_manager.bzl", "package_manager_repositories")
load("@distroless//package_manager:dpkg.bzl", "dpkg_list", "dpkg_src")

# Sets up package manager which we use build deploy images.
def _package_manager_setup():
    package_manager_repositories()

    dpkg_src(
        name = "debian_buster",
        arch = "amd64",
        distro = "buster",
        sha256 = "bd1bed6b19bf173d60ac130edee47087203e873f3b0981f5987f77a91a2cba85",
        snapshot = "20190716T085419Z",
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
        sources = ["@debian_buster//file:Packages.json"],
    )

def _docker_images_setup():
    _go_image_repos()
    _cc_image_repos()

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

def _artifacts_setup():
    http_file(
        name = "linux_headers_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-4.14.104-pl2.tar.gz"],
        sha256 = "dacd190bd5a7cf3d8a38f53fe13ff077512a0146cbb78b9e77dfbab05fce03bd",
        downloaded_file_path = "linux_headers.tar.gz",
    )

def pl_workspace_setup():
    gazelle_dependencies()
    buildifier_dependencies()
    grpc_deps()

    _package_manager_setup()
    _docker_images_setup()
    _artifacts_setup()
