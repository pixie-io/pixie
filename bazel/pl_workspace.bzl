load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
load("@com_github_bazelbuild_buildtools//buildifier:deps.bzl", "buildifier_dependencies")
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)
load("@io_bazel_rules_docker//go:image.bzl", _go_image_repos = "repositories")
load("@io_bazel_rules_docker//java:image.bzl", _java_image_repos = "repositories")
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
            "liblzma5",
            "libtinfo5",
            "zlib1g",
            "libsasl2-2",
            "libssl1.1",
            "libgcc1",
        ],
        sources = ["@debian_buster//file:Packages.json"],
    )

def _docker_images_setup():
    _go_image_repos()
    _java_image_repos()

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
        name = "linux_headers_4_14_176_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-4.14.176-pl1.tar.gz"],
        sha256 = "59b7414f39ee0d76041558594c2b67a0c818360f2c9184c2021c906d5ad54300",
        downloaded_file_path = "linux_headers.tar.gz",
    )

    http_file(
        name = "linux_headers_4_15_18_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-4.15.18-pl1.tar.gz"],
        sha256 = "0a1280e02aef58486634e4cbf17c1a4d9a650411e17a9b4587b7892efe89bcfc",
        downloaded_file_path = "linux_headers.tar.gz",
    )

    http_file(
        name = "linux_headers_4_16_18_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-4.16.18-pl1.tar.gz"],
        sha256 = "03a93b70b76d405170fa2ca8b640f9932741715b344bbab2acb233f8e65c27b0",
        downloaded_file_path = "linux_headers.tar.gz",
    )

    http_file(
        name = "linux_headers_4_17_19_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-4.17.19-pl1.tar.gz"],
        sha256 = "a7e0fd404360ef7a18a7809de2b3bc07a3e4667d1e753fafd0d441636178b44a",
        downloaded_file_path = "linux_headers.tar.gz",
    )

    http_file(
        name = "linux_headers_4_18_20_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-4.18.20-pl1.tar.gz"],
        sha256 = "04975487e731a966a6a0ab301ad6b306122c15f1cd783f752e5c99b6b23832bb",
        downloaded_file_path = "linux_headers.tar.gz",
    )

    http_file(
        name = "linux_headers_4_19_118_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-4.19.118-pl1.tar.gz"],
        sha256 = "5957b27bd2ee22197b00f006c9b8337a3577b6f54972728fdb98e262284c27bf",
        downloaded_file_path = "linux_headers.tar.gz",
    )

    http_file(
        name = "linux_headers_5_0_21_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-5.0.21-pl1.tar.gz"],
        sha256 = "f85d8dd0c2d1fa710afd796350d7bafeb636ed54653ecc6f7ba1fc18ea2ce9c2",
        downloaded_file_path = "linux_headers.tar.gz",
    )

    http_file(
        name = "linux_headers_5_1_21_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-5.1.21-pl1.tar.gz"],
        sha256 = "852e5898be4b61fe8c92aa4aada4747b678b361a57764e5cec6ed5bbfd423a9c",
        downloaded_file_path = "linux_headers.tar.gz",
    )

    http_file(
        name = "linux_headers_5_2_21_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-5.2.21-pl1.tar.gz"],
        sha256 = "2c8f605f38cb673eefba3efedf7b3d3a0bbcd3b67d391768ab09eea76f9dba7f",
        downloaded_file_path = "linux_headers.tar.gz",
    )

    http_file(
        name = "linux_headers_5_3_18_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-5.3.18-pl1.tar.gz"],
        sha256 = "0c83721bb2f42acdd2a0c54164d873749acae2c3a9696b599ce70396fc0ede3b",
        downloaded_file_path = "linux_headers.tar.gz",
    )

    http_file(
        name = "linux_headers_5_4_35_tar_gz",
        urls = ["https://storage.googleapis.com/pl-infra-dev-artifacts/linux-headers-5.4.35-pl1.tar.gz"],
        sha256 = "e9a732e6879728dfd667ef6917089c6d562b85334f46ce4b8aacd91ec307c9ae",
        downloaded_file_path = "linux_headers.tar.gz",
    )

# TODO(zasgar): remove this when downstream bugs relying on bazel version are removed.
def _impl(repository_ctx):
    bazel_verision_for_upb = "bazel_version = \"" + native.bazel_version + "\""
    bazel_version_for_foreign_cc = "BAZEL_VERSION = \"" + native.bazel_version + "\""
    repository_ctx.file("bazel_version.bzl", bazel_verision_for_upb)
    repository_ctx.file("def.bzl", bazel_version_for_foreign_cc)
    repository_ctx.file("BUILD", "")

bazel_version_repository = repository_rule(
    implementation = _impl,
    local = True,
)

def pl_workspace_setup():
    gazelle_dependencies()
    buildifier_dependencies()
    grpc_deps()

    bazel_version_repository(
        name = "bazel_version",
    )

    container_repositories()

    k8s_repositories()
    k8s_go_deps()

    _package_manager_setup()
    _docker_images_setup()
    _artifacts_setup()
