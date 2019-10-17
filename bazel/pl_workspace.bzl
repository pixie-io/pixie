load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
load("@com_github_bazelbuild_buildtools//buildifier:deps.bzl", "buildifier_dependencies")
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)
load("@io_bazel_rules_docker//go:image.bzl", _go_image_repos = "repositories")
load("@io_bazel_rules_docker//container:container.bzl", "container_pull")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
load("@distroless//package_manager:package_manager.bzl", "package_manager_repositories")
load("@distroless//package_manager:dpkg.bzl", "dpkg_list", "dpkg_src")
load("@io_bazel_rules_k8s//k8s:k8s.bzl", "k8s_repositories")
load("@io_bazel_rules_k8s//k8s:k8s_go_deps.bzl", k8s_go_deps = "deps")

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
            "libgcc1",
            "libgomp1",
            "liblzma5",
            "libstdc++6",
            "libtinfo5",
            "libunwind8",
            "zlib1g",
        ],
        sources = ["@debian_buster//file:Packages.json"],
    )

def _docker_images_setup():
    _go_image_repos()

    # Import NGINX repo.
    container_pull(
        name = "nginx_base",
        digest = "sha256:204a9a8e65061b10b92ad361dd6f406248404fe60efd5d6a8f2595f18bb37aad",
        registry = "index.docker.io",
        repository = "library/nginx",
    )

    container_pull(
        name = "base_image",
        digest = "sha256:e37cf3289c1332c5123cbf419a1657c8dad0811f2f8572433b668e13747718f8",
        registry = "gcr.io",
        repository = "distroless/base",
    )

    container_pull(
        name = "base_image_debug",
        digest = "sha256:f989df6099c5efb498021c7f01b74f484b46d2f5e1cdb862e508569d87569f2b",
        registry = "gcr.io",
        repository = "distroless/base",
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
    container_repositories()

    _package_manager_setup()
    _docker_images_setup()
    _artifacts_setup()
