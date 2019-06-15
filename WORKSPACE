workspace(name = "pl")

load("//:workspace.bzl", "check_min_bazel_version")

check_min_bazel_version("0.25.0")

load("//bazel:repositories.bzl", "pl_deps")

# Install Pixie Labs Dependencies.
pl_deps()

# The go dependency does not work if loaded during our normal pl_workspace setup.
# moving it here fixes the errors.
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(go_version = "host")

load("//bazel:pl_workspace.bzl", "pl_workspace_setup")

pl_workspace_setup()

load("//bazel:cc_configure.bzl", "cc_configure")
load("//bazel:gogo.bzl", "gogo_grpc_proto")
load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

cc_configure()

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")

gogo_grpc_proto(name = "gogo_grpc_proto")

# gazelle:repo bazel_gazelle
##########################################################
# Auto-generated GO dependencies (DO NOT EDIT).
##########################################################
go_repository(
    name = "com_github_golang_protobuf",
    commit = "d7fc20193620986259ffb1f9b9da752114ee14a4",
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

go_repository(
    name = "com_github_gorilla_context",
    commit = "08b5f424b9271eedf6f9f0ce86cb9396ed337a42",
    importpath = "github.com/gorilla/context",
)

go_repository(
    name = "com_github_gorilla_securecookie",
    commit = "e59506cc896acb7f7bf732d4fdf5e25f7ccd8983",
    importpath = "github.com/gorilla/securecookie",
)

go_repository(
    name = "com_github_gorilla_sessions",
    commit = "f57b7e2d29c6211d16ffa52a0998272f75799030",
    importpath = "github.com/gorilla/sessions",
)

go_repository(
    name = "com_github_grpc_ecosystem_go_grpc_middleware",
    commit = "c250d6563d4d4c20252cd865923440e829844f4e",
    importpath = "github.com/grpc-ecosystem/go-grpc-middleware",
)

go_repository(
    name = "com_github_abiosoft_ishell",
    commit = "0a6b1640e32638dd433ee7e57ec325a7bdd79561",
    importpath = "github.com/abiosoft/ishell",
)

go_repository(
    name = "com_github_abiosoft_readline",
    commit = "155bce2042db95a783081fab225e74dd879055b0",
    importpath = "github.com/abiosoft/readline",
)

go_repository(
    name = "com_github_apache_arrow",
    commit = "48f7b360b138d7152e418021864e7b574f02458c",
    importpath = "github.com/apache/arrow",
)

go_repository(
    name = "com_github_fatih_color",
    commit = "5b77d2a35fb0ede96d138fc9a99f5c9b6aef11b4",
    importpath = "github.com/fatih/color",
)

go_repository(
    name = "com_github_flynn_archive_go_shlex",
    commit = "3f9db97f856818214da2e1057f8ad84803971cff",
    importpath = "github.com/flynn-archive/go-shlex",
)

go_repository(
    name = "com_github_mattn_go_runewidth",
    commit = "3ee7d812e62a0804a7d0a324e0249ca2db3476d3",
    importpath = "github.com/mattn/go-runewidth",
)

go_repository(
    name = "com_github_olekukonko_tablewriter",
    commit = "e6d60cf7ba1f42d86d54cdf5508611c4aafb3970",
    importpath = "github.com/olekukonko/tablewriter",
)

go_repository(
    name = "com_github_tylerbrock_colorjson",
    commit = "95ec53f28296f47af86a81eb73f0d7fe2b23a322",
    importpath = "github.com/TylerBrock/colorjson",
)

go_repository(
    name = "com_github_ajstarks_svgo",
    commit = "6ce6a3bcf6cde6c5007887677ebd148ec30f42a4",
    importpath = "github.com/ajstarks/svgo",
)

go_repository(
    name = "com_github_fogleman_gg",
    commit = "0403632d5b905943a1c2a5b2763aaecd568467ec",
    importpath = "github.com/fogleman/gg",
)

go_repository(
    name = "com_github_golang_freetype",
    commit = "e2365dfdc4a05e4b8299a783240d4a7d5a65d4e4",
    importpath = "github.com/golang/freetype",
)

go_repository(
    name = "com_github_jung_kurt_gofpdf",
    commit = "8fd1e0a49c50a568249b877054a6401de1c118a8",
    importpath = "github.com/jung-kurt/gofpdf",
)

go_repository(
    name = "org_golang_x_exp",
    commit = "7f338f5710825d68a78f2a58c25378e5e2b972df",
    importpath = "golang.org/x/exp",
)

go_repository(
    name = "org_golang_x_image",
    commit = "3fc05d484e9f77dd51816890e05f2602e4ca4d65",
    importpath = "golang.org/x/image",
)

go_repository(
    name = "org_gonum_v1_gonum",
    commit = "5d695651a1d533b5b371458b6da17b45c17ed718",
    importpath = "gonum.org/v1/gonum",
)

go_repository(
    name = "org_gonum_v1_plot",
    commit = "3a5f52653745fa15b552c97e1d8052e496e85503",
    importpath = "gonum.org/v1/plot",
)

go_repository(
    name = "com_github_bmatcuk_doublestar",
    commit = "85a78806aa1b4707d1dbace9be592cf1ece91ab3",
    importpath = "github.com/bmatcuk/doublestar",
)

go_repository(
    name = "com_github_graph_gophers_graphql_go",
    commit = "3e8838d4614c12ab337e796548521744f921e05d",
    importpath = "github.com/graph-gophers/graphql-go",
)

go_repository(
    name = "com_github_nats_io_go_nats",
    commit = "70fe06cee50d4b6f98248d9675fb55f2a3aa7228",
    importpath = "github.com/nats-io/go-nats",
)

go_repository(
    name = "com_github_nats_io_nkeys",
    commit = "1546a3320a8f195a5b5c84aef8309377c2e411d5",
    importpath = "github.com/nats-io/nkeys",
)

go_repository(
    name = "com_github_nats_io_nuid",
    commit = "4b96681fa6d28dd0ab5fe79bac63b3a493d9ee94",
    importpath = "github.com/nats-io/nuid",
)

go_repository(
    name = "com_github_nats_io_gnatsd",
    commit = "3e64f0bfd1fe4c2cf6599f064ff72fa7af439663",
    importpath = "github.com/nats-io/gnatsd",
)

go_repository(
    name = "com_github_phayes_freeport",
    commit = "b8543db493a5ed890c5499e935e2cad7504f3a04",
    importpath = "github.com/phayes/freeport",
)

go_repository(
    name = "com_github_beorn7_perks",
    commit = "4b2b341e8d7715fae06375aa633dbb6e91b3fb46",
    importpath = "github.com/beorn7/perks",
)

go_repository(
    name = "com_github_coreos_bbolt",
    commit = "63597a96ec0ad9e6d43c3fc81e809909e0237461",
    importpath = "github.com/coreos/bbolt",
)

go_repository(
    name = "com_github_coreos_etcd",
    build_file_generation = "on",
    build_file_proto_mode = "disable",
    commit = "98d308426819d892e149fe45f6fd542464cb1f9d",
    importpath = "github.com/coreos/etcd",
)

go_repository(
    name = "com_github_coreos_go_semver",
    commit = "e214231b295a8ea9479f11b70b35d5acf3556d9b",
    importpath = "github.com/coreos/go-semver",
)

go_repository(
    name = "com_github_coreos_go_systemd",
    commit = "95778dfbb74eb7e4dbaf43bf7d71809650ef8076",
    importpath = "github.com/coreos/go-systemd",
)

go_repository(
    name = "com_github_coreos_pkg",
    commit = "97fdf19511ea361ae1c100dd393cc47f8dcfa1e1",
    importpath = "github.com/coreos/pkg",
)

go_repository(
    name = "com_github_ghodss_yaml",
    commit = "0ca9ea5df5451ffdf184b4428c902747c2c11cd7",
    importpath = "github.com/ghodss/yaml",
)

go_repository(
    name = "com_github_google_btree",
    commit = "4030bb1f1f0c35b30ca7009e9ebd06849dd45306",
    importpath = "github.com/google/btree",
)

go_repository(
    name = "com_github_gorilla_websocket",
    commit = "66b9c49e59c6c48f0ffce28c2d8b8a5678502c6d",
    importpath = "github.com/gorilla/websocket",
)

go_repository(
    name = "com_github_grpc_ecosystem_go_grpc_prometheus",
    commit = "c225b8c3b01faf2899099b768856a9e916e5087b",
    importpath = "github.com/grpc-ecosystem/go-grpc-prometheus",
)

go_repository(
    name = "com_github_grpc_ecosystem_grpc_gateway",
    commit = "8fd5fd9d19ce68183a6b0934519dfe7fe6269612",
    importpath = "github.com/grpc-ecosystem/grpc-gateway",
)

go_repository(
    name = "com_github_jonboulle_clockwork",
    commit = "2eee05ed794112d45db504eb05aa693efd2b8b09",
    importpath = "github.com/jonboulle/clockwork",
)

go_repository(
    name = "com_github_json_iterator_go",
    commit = "0ff49de124c6f76f8494e194af75bde0f1a49a29",
    importpath = "github.com/json-iterator/go",
)

go_repository(
    name = "com_github_matttproud_golang_protobuf_extensions",
    commit = "c12348ce28de40eed0136aa2b644d0ee0650e56c",
    importpath = "github.com/matttproud/golang_protobuf_extensions",
)

go_repository(
    name = "com_github_modern_go_concurrent",
    commit = "bacd9c7ef1dd9b15be4a9909b8ac7a4e313eec94",
    importpath = "github.com/modern-go/concurrent",
)

go_repository(
    name = "com_github_modern_go_reflect2",
    commit = "4b7aa43c6742a2c18fdef89dd197aaae7dac7ccd",
    importpath = "github.com/modern-go/reflect2",
)

go_repository(
    name = "com_github_petar_gollrb",
    commit = "33fb24c13b99c46c93183c291836c573ac382536",
    importpath = "github.com/petar/GoLLRB",
)

go_repository(
    name = "com_github_prometheus_client_golang",
    commit = "505eaef017263e299324067d40ca2c48f6a2cf50",
    importpath = "github.com/prometheus/client_golang",
)

go_repository(
    name = "com_github_prometheus_client_model",
    commit = "fd36f4220a901265f90734c3183c5f0c91daa0b8",
    importpath = "github.com/prometheus/client_model",
)

go_repository(
    name = "com_github_prometheus_common",
    commit = "1ba88736f028e37bc17328369e94a537ae9e0234",
    importpath = "github.com/prometheus/common",
)

go_repository(
    name = "com_github_prometheus_procfs",
    commit = "5867b95ac084bbfee6ea16595c4e05ab009021da",
    importpath = "github.com/prometheus/procfs",
)

go_repository(
    name = "com_github_soheilhy_cmux",
    commit = "e09e9389d85d8492d313d73d1469c029e710623f",
    importpath = "github.com/soheilhy/cmux",
)

go_repository(
    name = "com_github_tmc_grpc_websocket_proxy",
    commit = "0ad062ec5ee553a48f6dbd280b7a1b5638e8a113",
    importpath = "github.com/tmc/grpc-websocket-proxy",
)

go_repository(
    name = "com_github_xiang90_probing",
    commit = "43a291ad63a214a207fefbf03c7d9d78b703162b",
    importpath = "github.com/xiang90/probing",
)

go_repository(
    name = "org_golang_x_time",
    commit = "9d24e82272b4f38b78bc8cff74fa936d31ccd8ef",
    importpath = "golang.org/x/time",
)

go_repository(
    name = "org_uber_go_atomic",
    commit = "df976f2515e274675050de7b3f42545de80594fd",
    importpath = "go.uber.org/atomic",
)

go_repository(
    name = "org_uber_go_multierr",
    commit = "3c4937480c32f4c13a875a1829af76c98ca3d40a",
    importpath = "go.uber.org/multierr",
)

go_repository(
    name = "org_uber_go_zap",
    commit = "27376062155ad36be76b0f12cf1572a221d3a48c",
    importpath = "go.uber.org/zap",
)

go_repository(
    name = "com_github_google_go_cmp",
    commit = "6f77996f0c42f7b84e5a2b252227263f93432e9b",
    importpath = "github.com/google/go-cmp",
)

go_repository(
    name = "com_github_google_gofuzz",
    commit = "f140a6486e521aad38f5917de355cbf147cc0496",
    importpath = "github.com/google/gofuzz",
)

go_repository(
    name = "com_github_googleapis_gnostic",
    commit = "7c663266750e7d82587642f65e60bc4083f1f84e",
    importpath = "github.com/googleapis/gnostic",
)

go_repository(
    name = "com_github_hashicorp_golang_lru",
    commit = "7087cb70de9f7a8bc0a10c375cb0d2280a8edf9c",
    importpath = "github.com/hashicorp/golang-lru",
)

go_repository(
    name = "com_github_imdario_mergo",
    commit = "7c29201646fa3de8506f701213473dd407f19646",
    importpath = "github.com/imdario/mergo",
)

go_repository(
    name = "com_google_cloud_go",
    commit = "775730d6e48254a2430366162cf6298e5368833c",
    importpath = "cloud.google.com/go",
)

go_repository(
    name = "in_gopkg_inf_v0",
    commit = "d2d2541c53f18d2a059457998ce2876cc8e67cbf",
    importpath = "gopkg.in/inf.v0",
)

go_repository(
    name = "io_k8s_api",
    build_file_proto_mode = "disable",
    commit = "67ef80593b248c87b40adcbff0d564819f27640a",
    importpath = "k8s.io/api",
)

go_repository(
    name = "io_k8s_apimachinery",
    build_file_proto_mode = "disable",
    commit = "6a84e37a896db9780c75367af8d2ed2bb944022e",
    importpath = "k8s.io/apimachinery",
)

go_repository(
    name = "io_k8s_client_go",
    commit = "6ee68ca5fd8355d024d02f9db0b3b667e8357a0f",
    importpath = "k8s.io/client-go",
)

go_repository(
    name = "io_k8s_klog",
    commit = "78315d914a8af2453db4864e69230b647b1ff711",
    importpath = "k8s.io/klog",
)

go_repository(
    name = "io_k8s_sigs_yaml",
    commit = "fd68e9863619f6ec2fdd8625fe1f02e7c877e480",
    importpath = "sigs.k8s.io/yaml",
)

go_repository(
    name = "io_k8s_utils",
    commit = "6999998975a717e7f5fc2a7476497044cb111854",
    importpath = "k8s.io/utils",
)

go_repository(
    name = "org_golang_google_appengine",
    commit = "4c25cacc810c02874000e4f7071286a8e96b2515",
    importpath = "google.golang.org/appengine",
)

go_repository(
    name = "org_golang_x_oauth2",
    commit = "aaccbc9213b0974828f81aaac109d194880e3014",
    importpath = "golang.org/x/oauth2",
)

go_repository(
    name = "io_etcd_go_etcd",
    commit = "98d308426819d892e149fe45f6fd542464cb1f9d",
    importpath = "go.etcd.io/etcd",
)
