workspace(name = "pl")

load("//:workspace.bzl", "check_min_bazel_version")

check_min_bazel_version("0.17.1")

##########################################################
# Bazel Go setup.
##########################################################
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "io_bazel_rules_go",
    sha256 = "97cf62bdef33519412167fd1e4b0810a318a7c234f5f8dc4f53e2da86241c492",
    urls = ["https://github.com/bazelbuild/rules_go/releases/download/0.15.3/rules_go-0.15.3.tar.gz"],
)

http_archive(
    name = "bazel_gazelle",
    sha256 = "c0a5739d12c6d05b6c1ad56f2200cb0b57c5a70e03ebd2f7b87ce88cabf09c7b",
    urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/0.14.0/bazel-gazelle-0.14.0.tar.gz"],
)

load("@io_bazel_rules_go//go:def.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains()

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")

gazelle_dependencies()

##########################################################
# Bazel Buildtools setup.
##########################################################
http_archive(
    name = "com_github_bazelbuild_buildtools",
    strip_prefix = "buildtools-4a7914a1466ff7388c934bfcd43a3852928536f6",
    url = "https://github.com/bazelbuild/buildtools/archive/4a7914a1466ff7388c934bfcd43a3852928536f6.zip",
)

load("@com_github_bazelbuild_buildtools//buildifier:deps.bzl", "buildifier_dependencies")

buildifier_dependencies()

##########################################################
# Bazel CC setup.
##########################################################
# Google test related rules.
new_http_archive(
    name = "googletest",
    build_file = "third_party/gtest.BUILD",
    strip_prefix = "googletest-release-1.8.0",
    url = "https://github.com/google/googletest/archive/release-1.8.0.zip",
)

bind(
    name = "gtest",
    actual = "@googletest//:gtest",
)

bind(
    name = "gtest-main",
    actual = "@googletest//:gtest-main",
)

# GRPC.
git_repository(
    name = "com_github_grpc_grpc",
    commit = "d8020cb6daa87f1a3bb3b0c299bc081c4a3de1e8",
    remote = "https://github.com/grpc/grpc.git",
)

load("@com_github_grpc_grpc//:bazel/grpc_deps.bzl", "grpc_deps")

grpc_deps()

##########################################################
# Docker setup.
##########################################################

http_archive(
    name = "io_bazel_rules_docker",
    sha256 = "29d109605e0d6f9c892584f07275b8c9260803bf0c6fcb7de2623b2bedc910bd",
    strip_prefix = "rules_docker-0.5.1",
    urls = ["https://github.com/bazelbuild/rules_docker/archive/v0.5.1.tar.gz"],
)

load(
    "@io_bazel_rules_docker//go:image.bzl",
    _go_image_repos = "repositories",
)

_go_image_repos()

load(
    "@io_bazel_rules_docker//cc:image.bzl",
    _cc_image_repos = "repositories",
)

_cc_image_repos()

# Import NGINX repo.
load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_pull",
    container_repositories = "repositories",
)

container_pull(
    name = "nginx_base",
    digest = "sha256:9ad0746d8f2ea6df3a17ba89eca40b48c47066dfab55a75e08e2b70fc80d929e",
    registry = "index.docker.io",
    repository = "library/nginx",
)

##########################################################
# Auto-generated GO dependencies (DO NOT EDIT).
##########################################################
go_repository(
    name = "com_github_golang_protobuf",
    commit = "b4deda0973fb4c70b50d226b1af49f3da59f5265",
    importpath = "github.com/golang/protobuf",
)

go_repository(
    name = "org_golang_google_genproto",
    commit = "383e8b2c3b9e36c4076b235b32537292176bae20",
    importpath = "google.golang.org/genproto",
)

go_repository(
    name = "org_golang_google_grpc",
    commit = "32fb0ac620c32ba40a4626ddf94d90d12cce3455",
    importpath = "google.golang.org/grpc",
)

go_repository(
    name = "org_golang_x_net",
    commit = "c39426892332e1bb5ec0a434a079bf82f5d30c54",
    importpath = "golang.org/x/net",
)

go_repository(
    name = "org_golang_x_sys",
    commit = "4e1fef5609515ec7a2cee7b5de30ba6d9b438cbf",
    importpath = "golang.org/x/sys",
)

go_repository(
    name = "org_golang_x_text",
    commit = "f21a4dfb5e38f5895301dc265a8def02365cc3d0",
    importpath = "golang.org/x/text",
)

go_repository(
    name = "com_github_google_uuid",
    commit = "d460ce9f8df2e77fb1ba55ca87fafed96c607494",
    importpath = "github.com/google/uuid",
)

go_repository(
    name = "org_golang_x_sync",
    commit = "1d60e4601c6fd243af51cc01ddf169918a5407ca",
    importpath = "golang.org/x/sync",
)

go_repository(
    name = "com_github_davecgh_go_spew",
    commit = "8991bc29aa16c548c550c7ff78260e27b9ab7c73",
    importpath = "github.com/davecgh/go-spew",
)

go_repository(
    name = "com_github_pmezard_go_difflib",
    commit = "792786c7400a136282c1664665ae0a8db921c6c2",
    importpath = "github.com/pmezard/go-difflib",
)

go_repository(
    name = "com_github_stretchr_testify",
    commit = "f35b8ab0b5a2cef36673838d662e249dd9c94686",
    importpath = "github.com/stretchr/testify",
)

go_repository(
    name = "com_github_fsnotify_fsnotify",
    commit = "c2828203cd70a50dcccfb2761f8b1f8ceef9a8e9",
    importpath = "github.com/fsnotify/fsnotify",
)

go_repository(
    name = "com_github_hashicorp_hcl",
    commit = "8cb6e5b959231cc1119e43259c4a608f9c51a241",
    importpath = "github.com/hashicorp/hcl",
)

go_repository(
    name = "com_github_inconshreveable_mousetrap",
    commit = "76626ae9c91c4f2a10f34cad8ce83ea42c93bb75",
    importpath = "github.com/inconshreveable/mousetrap",
)

go_repository(
    name = "com_github_magiconair_properties",
    commit = "c2353362d570a7bfa228149c62842019201cfb71",
    importpath = "github.com/magiconair/properties",
)

go_repository(
    name = "com_github_mitchellh_mapstructure",
    commit = "fe40af7a9c397fa3ddba203c38a5042c5d0475ad",
    importpath = "github.com/mitchellh/mapstructure",
)

go_repository(
    name = "com_github_pelletier_go_toml",
    commit = "c01d1270ff3e442a8a57cddc1c92dc1138598194",
    importpath = "github.com/pelletier/go-toml",
)

go_repository(
    name = "com_github_spf13_afero",
    commit = "d40851caa0d747393da1ffb28f7f9d8b4eeffebd",
    importpath = "github.com/spf13/afero",
)

go_repository(
    name = "com_github_spf13_cast",
    commit = "8965335b8c7107321228e3e3702cab9832751bac",
    importpath = "github.com/spf13/cast",
)

go_repository(
    name = "com_github_spf13_cobra",
    commit = "ef82de70bb3f60c65fb8eebacbb2d122ef517385",
    importpath = "github.com/spf13/cobra",
)

go_repository(
    name = "com_github_spf13_jwalterweatherman",
    commit = "4a4406e478ca629068e7768fc33f3f044173c0a6",
    importpath = "github.com/spf13/jwalterweatherman",
)

go_repository(
    name = "com_github_spf13_pflag",
    commit = "9a97c102cda95a86cec2345a6f09f55a939babf5",
    importpath = "github.com/spf13/pflag",
)

go_repository(
    name = "com_github_spf13_viper",
    commit = "2c12c60302a5a0e62ee102ca9bc996277c2f64f5",
    importpath = "github.com/spf13/viper",
)

go_repository(
    name = "in_gopkg_yaml_v2",
    commit = "5420a8b6744d3b0345ab293f6fcba19c978f1183",
    importpath = "gopkg.in/yaml.v2",
)

go_repository(
    name = "com_github_blang_semver",
    commit = "2ee87856327ba09384cabd113bc6b5d174e9ec0f",
    importpath = "github.com/blang/semver",
)

go_repository(
    name = "com_github_c9s_goprocinfo",
    commit = "0010a05ce49fde7f50669bc7ecda7d41dd6ab824",
    importpath = "github.com/c9s/goprocinfo",
)

go_repository(
    name = "com_github_gogo_protobuf",
    commit = "636bf0302bc95575d69441b25a2603156ffdddf1",
    importpath = "github.com/gogo/protobuf",
)

go_repository(
    name = "com_github_graph_gophers_graphql_go",
    commit = "25d6d94fa7a7f2b0ce10fd509e54e85f7a2f866b",
    importpath = "github.com/graph-gophers/graphql-go",
)

go_repository(
    name = "com_github_konsorten_go_windows_terminal_sequences",
    commit = "b729f2633dfe35f4d1d8a32385f6685610ce1cb5",
    importpath = "github.com/konsorten/go-windows-terminal-sequences",
)

go_repository(
    name = "com_github_opentracing_opentracing_go",
    commit = "1949ddbfd147afd4d964a9f00b24eb291e0e7c38",
    importpath = "github.com/opentracing/opentracing-go",
)

go_repository(
    name = "com_github_satori_go_uuid",
    commit = "f58768cc1a7a7e77a3bd49e98cdd21419399b6a3",
    importpath = "github.com/satori/go.uuid",
)

go_repository(
    name = "com_github_sirupsen_logrus",
    commit = "a67f783a3814b8729bd2dac5780b5f78f8dbd64d",
    importpath = "github.com/sirupsen/logrus",
)

go_repository(
    name = "org_golang_x_crypto",
    commit = "e3636079e1a4c1f337f212cc5cd2aca108f6c900",
    importpath = "golang.org/x/crypto",
)

go_repository(
    name = "com_github_dgrijalva_jwt_go",
    commit = "06ea1031745cb8b3dab3f6a236daf2b0aa468b7e",
    importpath = "github.com/dgrijalva/jwt-go",
)

go_repository(
    name = "com_github_golang_mock",
    commit = "c34cdb4725f4c3844d095133c6e40e448b86589b",
    importpath = "github.com/golang/mock",
)

go_repository(
    name = "com_github_zenazn_goji",
    commit = "64eb34159fe53473206c2b3e70fe396a639452f2",
    importpath = "github.com/zenazn/goji",
)

go_repository(
    name = "com_github_mattn_go_colorable",
    commit = "167de6bfdfba052fa6b2d3664c8f5272e23c9072",
    importpath = "github.com/mattn/go-colorable",
)

go_repository(
    name = "com_github_mattn_go_isatty",
    commit = "6ca4dbf54d38eea1a992b3c722a76a5d1c4cb25c",
    importpath = "github.com/mattn/go-isatty",
)

go_repository(
    name = "com_github_mgutz_ansi",
    commit = "9520e82c474b0a04dd04f8a40959027271bab992",
    importpath = "github.com/mgutz/ansi",
)

go_repository(
    name = "com_github_x_cray_logrus_prefixed_formatter",
    commit = "bb2702d423886830dee131692131d35648c382e2",
    importpath = "github.com/x-cray/logrus-prefixed-formatter",
)
