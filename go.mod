module px.dev/pixie

go 1.17

require (
	cloud.google.com/go/bigquery v1.18.0
	cloud.google.com/go/storage v1.10.0
	github.com/EvilSuperstars/go-cidrman v0.0.0-20190607145828-28e79e32899a
	github.com/Masterminds/sprig/v3 v3.2.2
	github.com/PuerkitoBio/goquery v1.6.0
	github.com/alecthomas/chroma v0.7.1
	github.com/alecthomas/participle v0.4.1
	github.com/badoux/checkmail v0.0.0-20181210160741-9661bd69e9ad
	github.com/bazelbuild/rules_go v0.22.4
	github.com/blang/semver v3.5.1+incompatible
	github.com/bmatcuk/doublestar v1.2.2
	github.com/bsm/go-vlq v0.0.0-20150828105119-ec6e8d4f5f4e
	github.com/cenkalti/backoff/v3 v3.2.2
	github.com/cockroachdb/pebble v0.0.0-20210120202502-6110b03a8a85
	github.com/dgraph-io/badger/v3 v3.2011.1
	github.com/dustin/go-humanize v1.0.0
	github.com/emicklei/dot v0.10.1
	github.com/evanphx/json-patch v4.9.0+incompatible
	github.com/fatih/color v1.10.0
	github.com/gdamore/tcell v1.3.0
	github.com/getsentry/sentry-go v0.11.0
	github.com/go-openapi/runtime v0.19.26
	github.com/go-openapi/strfmt v0.20.0
	github.com/gofrs/uuid v4.0.0+incompatible
	github.com/gogo/protobuf v1.3.2
	github.com/golang-migrate/migrate v3.5.4+incompatible
	github.com/golang/mock v1.5.0
	github.com/google/go-github/v32 v32.1.0
	github.com/googleapis/google-cloud-go-testing v0.0.0-20191008195207-8e1d251e947d
	github.com/gorilla/handlers v1.5.1
	github.com/gorilla/sessions v1.2.1
	github.com/graph-gophers/graphql-go v1.3.0
	github.com/grpc-ecosystem/go-grpc-middleware v1.3.0
	github.com/ianlancetaylor/cgosymbolizer v0.0.0-20200424224625-be1b05b0b279
	github.com/inconshreveable/go-update v0.0.0-20160112193335-8152e7eb6ccf
	github.com/jackc/pgx v3.5.0+incompatible
	github.com/jmoiron/sqlx v1.2.0
	github.com/kardianos/osext v0.0.0-20190222173326-2bc1f35cddc0
	github.com/lestrrat-go/jwx v1.2.17
	github.com/lib/pq v1.10.4
	github.com/mattn/go-runewidth v0.0.9
	github.com/nats-io/nats-server/v2 v2.7.3
	github.com/nats-io/nats-streaming-server v0.24.2
	github.com/nats-io/nats.go v1.13.1-0.20220121202836-972a071d373d
	github.com/nats-io/stan.go v0.10.2
	github.com/olekukonko/tablewriter v0.0.5
	github.com/olivere/elastic/v7 v7.0.12
	github.com/ory/dockertest/v3 v3.8.1
	github.com/ory/hydra-client-go v1.9.2
	github.com/ory/kratos-client-go v0.5.4-alpha.1
	github.com/phayes/freeport v0.0.0-20171002181615-b8543db493a5
	github.com/prometheus/client_golang v1.11.0
	github.com/prometheus/common v0.26.0
	github.com/prometheus/prometheus v2.5.0+incompatible
	github.com/rivo/tview v0.0.0-20200404204604-ca37f83cb2e7
	github.com/rivo/uniseg v0.1.0
	github.com/sahilm/fuzzy v0.1.0
	github.com/sercand/kuberesolver/v3 v3.0.0
	github.com/sirupsen/logrus v1.8.1
	github.com/skratchdot/open-golang v0.0.0-20190402232053-79abb63cd66e
	github.com/spf13/cast v1.3.1
	github.com/spf13/cobra v1.2.1
	github.com/spf13/pflag v1.0.5
	github.com/spf13/viper v1.8.1
	github.com/stretchr/testify v1.7.0
	github.com/tidwall/buntdb v1.2.9
	github.com/txn2/txeh v1.2.1
	github.com/vbauerster/mpb/v4 v4.11.0
	github.com/zenazn/goji v0.9.1-0.20160507202103-64eb34159fe5
	go.etcd.io/etcd/api/v3 v3.5.0
	go.etcd.io/etcd/client/pkg/v3 v3.5.0
	go.etcd.io/etcd/client/v3 v3.5.0
	golang.org/x/net v0.0.0-20210330230544-e57232859fb2
	golang.org/x/oauth2 v0.0.0-20210313182246-cd4f82c27b84
	golang.org/x/sync v0.0.0-20210220032951-036812b2e83c
	golang.org/x/sys v0.0.0-20220224120231-95c6836cb0e7
	golang.org/x/term v0.0.0-20210317153231-de623e64d2a6
	google.golang.org/api v0.46.0
	google.golang.org/genproto v0.0.0-20210602131652-f16073e35f0c
	google.golang.org/grpc v1.38.0
	gopkg.in/launchdarkly/go-sdk-common.v2 v2.5.0
	gopkg.in/launchdarkly/go-server-sdk.v5 v5.8.1
	gopkg.in/segmentio/analytics-go.v3 v3.1.0
	gopkg.in/src-d/go-git.v4 v4.13.1
	gopkg.in/yaml.v2 v2.4.0
	k8s.io/api v0.20.6
	k8s.io/apimachinery v0.20.6
	k8s.io/cli-runtime v0.20.6
	k8s.io/client-go v0.20.6
	k8s.io/klog/v2 v2.8.0
	k8s.io/kubectl v0.20.6
	sigs.k8s.io/controller-runtime v0.8.3
	sigs.k8s.io/yaml v1.2.0
)

require (
	cloud.google.com/go v0.81.0 // indirect
	github.com/Azure/go-ansiterm v0.0.0-20170929234023-d6e3b3328b78 // indirect
	github.com/Azure/go-autorest v14.2.0+incompatible // indirect
	github.com/Azure/go-autorest/autorest v0.11.1 // indirect
	github.com/Azure/go-autorest/autorest/adal v0.9.5 // indirect
	github.com/Azure/go-autorest/autorest/date v0.3.0 // indirect
	github.com/Azure/go-autorest/logger v0.2.0 // indirect
	github.com/Azure/go-autorest/tracing v0.6.0 // indirect
	github.com/DataDog/zstd v1.4.1 // indirect
	github.com/MakeNowJust/heredoc v0.0.0-20170808103936-bb23615498cd // indirect
	github.com/Masterminds/goutils v1.1.1 // indirect
	github.com/Masterminds/semver/v3 v3.1.1 // indirect
	github.com/Microsoft/go-winio v0.5.1 // indirect
	github.com/Nvveen/Gotty v0.0.0-20120604004816-cd527374f1e5 // indirect
	github.com/PuerkitoBio/purell v1.1.1 // indirect
	github.com/PuerkitoBio/urlesc v0.0.0-20170810143723-de5bf2ad4578 // indirect
	github.com/VividCortex/ewma v1.1.1 // indirect
	github.com/andybalholm/cascadia v1.1.0 // indirect
	github.com/armon/go-metrics v0.0.0-20190430140413-ec5e00d3c878 // indirect
	github.com/asaskevich/govalidator v0.0.0-20200907205600-7a23bdc65eef // indirect
	github.com/beorn7/perks v1.0.1 // indirect
	github.com/bmizerany/assert v0.0.0-20160611221934-b7ed37b82869 // indirect
	github.com/cenkalti/backoff/v4 v4.1.2 // indirect
	github.com/cespare/xxhash v1.1.0 // indirect
	github.com/cespare/xxhash/v2 v2.1.1 // indirect
	github.com/chai2010/gettext-go v0.0.0-20160711120539-c6fed771bfd5 // indirect
	github.com/cockroachdb/apd v1.1.0 // indirect
	github.com/cockroachdb/errors v1.8.1 // indirect
	github.com/cockroachdb/logtags v0.0.0-20190617123548-eb05cc24525f // indirect
	github.com/cockroachdb/redact v1.0.8 // indirect
	github.com/cockroachdb/sentry-go v0.6.1-cockroachdb.2 // indirect
	github.com/containerd/containerd v1.5.5 // indirect
	github.com/containerd/continuity v0.1.0 // indirect
	github.com/coreos/go-semver v0.3.0 // indirect
	github.com/coreos/go-systemd/v22 v22.3.2 // indirect
	github.com/danwakefield/fnmatch v0.0.0-20160403171240-cbb64ac3d964 // indirect
	github.com/davecgh/go-spew v1.1.1 // indirect
	github.com/decred/dcrd/dcrec/secp256k1/v4 v4.0.0-20210816181553-5444fa50b93d // indirect
	github.com/dgraph-io/ristretto v0.0.4-0.20210122082011-bb5d392ed82d // indirect
	github.com/dlclark/regexp2 v1.1.6 // indirect
	github.com/docker/cli v20.10.11+incompatible // indirect
	github.com/docker/docker v20.10.7+incompatible // indirect
	github.com/docker/go-connections v0.4.0 // indirect
	github.com/docker/go-units v0.4.0 // indirect
	github.com/docker/spdystream v0.0.0-20160310174837-449fdfce4d96 // indirect
	github.com/emicklei/go-restful v2.9.5+incompatible // indirect
	github.com/emirpasic/gods v1.12.0 // indirect
	github.com/exponent-io/jsonpath v0.0.0-20151013193312-d6023ce2651d // indirect
	github.com/felixge/httpsnoop v1.0.1 // indirect
	github.com/form3tech-oss/jwt-go v3.2.2+incompatible // indirect
	github.com/fsnotify/fsnotify v1.4.9 // indirect
	github.com/gdamore/encoding v1.0.0 // indirect
	github.com/ghodss/yaml v1.0.0 // indirect
	github.com/go-logr/logr v0.4.0 // indirect
	github.com/go-openapi/analysis v0.19.16 // indirect
	github.com/go-openapi/errors v0.19.9 // indirect
	github.com/go-openapi/jsonpointer v0.19.5 // indirect
	github.com/go-openapi/jsonreference v0.19.3 // indirect
	github.com/go-openapi/loads v0.20.0 // indirect
	github.com/go-openapi/spec v0.20.0 // indirect
	github.com/go-openapi/swag v0.19.13 // indirect
	github.com/go-openapi/validate v0.20.1 // indirect
	github.com/go-stack/stack v1.8.0 // indirect
	github.com/goccy/go-json v0.9.1 // indirect
	github.com/golang/groupcache v0.0.0-20200121045136-8c9f03a8e57e // indirect
	github.com/golang/protobuf v1.5.2 // indirect
	github.com/golang/snappy v0.0.2-0.20190904063534-ff6b7dc882cf // indirect
	github.com/google/btree v1.0.0 // indirect
	github.com/google/flatbuffers v1.12.0 // indirect
	github.com/google/go-cmp v0.5.5 // indirect
	github.com/google/go-querystring v1.0.0 // indirect
	github.com/google/gofuzz v1.1.0 // indirect
	github.com/google/shlex v0.0.0-20191202100458-e7afc7fbc510 // indirect
	github.com/google/uuid v1.2.0 // indirect
	github.com/googleapis/gax-go/v2 v2.0.5 // indirect
	github.com/googleapis/gnostic v0.5.1 // indirect
	github.com/gorilla/securecookie v1.1.1 // indirect
	github.com/gregjones/httpcache v0.0.0-20180305231024-9cad4c3443a7 // indirect
	github.com/grpc-ecosystem/grpc-gateway v1.16.0 // indirect
	github.com/hashicorp/go-hclog v1.1.0 // indirect
	github.com/hashicorp/go-immutable-radix v1.0.0 // indirect
	github.com/hashicorp/go-msgpack v1.1.5 // indirect
	github.com/hashicorp/golang-lru v0.5.4 // indirect
	github.com/hashicorp/hcl v1.0.0 // indirect
	github.com/hashicorp/raft v1.3.5 // indirect
	github.com/huandu/xstrings v1.3.1 // indirect
	github.com/imdario/mergo v0.3.12 // indirect
	github.com/inconshreveable/mousetrap v1.0.0 // indirect
	github.com/jackc/fake v0.0.0-20150926172116-812a484cc733 // indirect
	github.com/jbenet/go-context v0.0.0-20150711004518-d14ea06fba99 // indirect
	github.com/josharian/intern v1.0.0 // indirect
	github.com/json-iterator/go v1.1.11 // indirect
	github.com/jstemmer/go-junit-report v0.9.1 // indirect
	github.com/kevinburke/ssh_config v0.0.0-20190725054713-01f96b0aa0cd // indirect
	github.com/klauspost/compress v1.14.4 // indirect
	github.com/kr/pretty v0.2.1 // indirect
	github.com/kr/text v0.2.0 // indirect
	github.com/kylelemons/godebug v1.1.0 // indirect
	github.com/launchdarkly/ccache v1.1.0 // indirect
	github.com/launchdarkly/eventsource v1.6.2 // indirect
	github.com/launchdarkly/go-semver v1.0.2 // indirect
	github.com/lestrrat-go/backoff/v2 v2.0.8 // indirect
	github.com/lestrrat-go/blackmagic v1.0.0 // indirect
	github.com/lestrrat-go/httpcc v1.0.0 // indirect
	github.com/lestrrat-go/iter v1.0.1 // indirect
	github.com/lestrrat-go/option v1.0.0 // indirect
	github.com/liggitt/tabwriter v0.0.0-20181228230101-89fcab3d43de // indirect
	github.com/lucasb-eyer/go-colorful v1.0.3 // indirect
	github.com/magiconair/properties v1.8.5 // indirect
	github.com/mailru/easyjson v0.7.6 // indirect
	github.com/mattn/go-colorable v0.1.8 // indirect
	github.com/mattn/go-isatty v0.0.12 // indirect
	github.com/matttproud/golang_protobuf_extensions v1.0.2-0.20181231171920-c182affec369 // indirect
	github.com/minio/highwayhash v1.0.2 // indirect
	github.com/mitchellh/copystructure v1.0.0 // indirect
	github.com/mitchellh/go-homedir v1.1.0 // indirect
	github.com/mitchellh/go-wordwrap v1.0.0 // indirect
	github.com/mitchellh/mapstructure v1.4.1 // indirect
	github.com/mitchellh/reflectwalk v1.0.0 // indirect
	github.com/moby/term v0.0.0-20201216013528-df9cb8a40635 // indirect
	github.com/modern-go/concurrent v0.0.0-20180306012644-bacd9c7ef1dd // indirect
	github.com/modern-go/reflect2 v1.0.1 // indirect
	github.com/nats-io/jwt/v2 v2.2.1-0.20220113022732-58e87895b296 // indirect
	github.com/nats-io/nkeys v0.3.0 // indirect
	github.com/nats-io/nuid v1.0.1 // indirect
	github.com/onsi/gomega v1.10.3 // indirect
	github.com/opencontainers/go-digest v1.0.0 // indirect
	github.com/opencontainers/image-spec v1.0.2 // indirect
	github.com/opencontainers/runc v1.0.2 // indirect
	github.com/opentracing/opentracing-go v1.2.0 // indirect
	github.com/patrickmn/go-cache v2.1.0+incompatible // indirect
	github.com/pelletier/go-toml v1.9.3 // indirect
	github.com/peterbourgon/diskv v2.0.1+incompatible // indirect
	github.com/pkg/errors v0.9.1 // indirect
	github.com/pmezard/go-difflib v1.0.0 // indirect
	github.com/prometheus/client_model v0.2.0 // indirect
	github.com/prometheus/procfs v0.7.3 // indirect
	github.com/russross/blackfriday v1.5.2 // indirect
	github.com/satori/go.uuid v1.2.0 // indirect
	github.com/segmentio/backo-go v0.0.0-20200129164019-23eae7c10bd3 // indirect
	github.com/sergi/go-diff v1.1.0 // indirect
	github.com/shopspring/decimal v1.2.0 // indirect
	github.com/spf13/afero v1.6.0 // indirect
	github.com/spf13/jwalterweatherman v1.1.0 // indirect
	github.com/src-d/gcfg v1.4.0 // indirect
	github.com/subosito/gotenv v1.2.0 // indirect
	github.com/tidwall/btree v1.1.0 // indirect
	github.com/tidwall/gjson v1.12.1 // indirect
	github.com/tidwall/grect v0.1.4 // indirect
	github.com/tidwall/match v1.1.1 // indirect
	github.com/tidwall/pretty v1.2.0 // indirect
	github.com/tidwall/rtred v0.1.2 // indirect
	github.com/tidwall/tinyqueue v0.1.1 // indirect
	github.com/xanzy/ssh-agent v0.2.1 // indirect
	github.com/xeipuuv/gojsonpointer v0.0.0-20180127040702-4e3ac2762d5f // indirect
	github.com/xeipuuv/gojsonreference v0.0.0-20180127040603-bd5ef7bd5415 // indirect
	github.com/xeipuuv/gojsonschema v1.2.0 // indirect
	github.com/xtgo/uuid v0.0.0-20140804021211-a0b114877d4c // indirect
	go.etcd.io/bbolt v1.3.6 // indirect
	go.mongodb.org/mongo-driver v1.4.3 // indirect
	go.opencensus.io v0.23.0 // indirect
	go.uber.org/atomic v1.7.0 // indirect
	go.uber.org/multierr v1.6.0 // indirect
	go.uber.org/zap v1.17.0 // indirect
	golang.org/x/crypto v0.0.0-20220214200702-86341886e292 // indirect
	golang.org/x/exp v0.0.0-20200513190911-00229845015e // indirect
	golang.org/x/lint v0.0.0-20201208152925-83fdc39ff7b5 // indirect
	golang.org/x/mod v0.3.1-0.20200828183125-ce943fd02449 // indirect
	golang.org/x/text v0.3.5 // indirect
	golang.org/x/time v0.0.0-20211116232009-f0f3c7e86c11 // indirect
	golang.org/x/tools v0.1.0 // indirect
	golang.org/x/xerrors v0.0.0-20200804184101-5ec99f83aff1 // indirect
	gomodules.xyz/jsonpatch/v2 v2.1.0 // indirect
	google.golang.org/appengine v1.6.7 // indirect
	google.golang.org/protobuf v1.26.0 // indirect
	gopkg.in/inf.v0 v0.9.1 // indirect
	gopkg.in/ini.v1 v1.62.0 // indirect
	gopkg.in/launchdarkly/go-jsonstream.v1 v1.0.1 // indirect
	gopkg.in/launchdarkly/go-sdk-events.v1 v1.1.1 // indirect
	gopkg.in/launchdarkly/go-server-sdk-evaluation.v1 v1.5.0 // indirect
	gopkg.in/src-d/go-billy.v4 v4.3.2 // indirect
	gopkg.in/warnings.v0 v0.1.2 // indirect
	gopkg.in/yaml.v3 v3.0.0-20210107192922-496545a6307b // indirect
	k8s.io/apiextensions-apiserver v0.20.1 // indirect
	k8s.io/component-base v0.20.6 // indirect
	k8s.io/kube-openapi v0.0.0-20201113171705-d219536bb9fd // indirect
	k8s.io/utils v0.0.0-20210111153108-fddb29f9d009 // indirect
	sigs.k8s.io/kustomize v2.0.3+incompatible // indirect
	sigs.k8s.io/structured-merge-diff/v4 v4.0.3 // indirect
)

replace (
	cloud.google.com/go => cloud.google.com/go v0.80.0
	github.com/Azure/go-ansiterm => github.com/Azure/go-ansiterm v0.0.0-20170929234023-d6e3b3328b78
	github.com/Azure/go-autorest => github.com/Azure/go-autorest v14.2.0+incompatible
	github.com/Azure/go-autorest/autorest => github.com/Azure/go-autorest/autorest v0.11.1
	github.com/Sirupsen/logrus => github.com/sirupsen/logrus v1.7.0
	github.com/docker/docker => github.com/moby/moby v20.10.5+incompatible
	github.com/go-openapi/analysis => github.com/go-openapi/analysis v0.19.5
	github.com/go-openapi/loads => github.com/go-openapi/loads v0.19.0
	github.com/go-openapi/spec => github.com/go-openapi/spec v0.19.3
	github.com/go-openapi/strfmt => github.com/go-openapi/strfmt v0.20.0
	github.com/golang/protobuf => github.com/golang/protobuf v1.5.2
	github.com/google/go-cmp => github.com/google/go-cmp v0.5.5
	github.com/spf13/cobra => github.com/spf13/cobra v1.2.1
	github.com/spf13/viper => github.com/spf13/viper v1.8.1
	go.mongodb.org/mongo-driver => go.mongodb.org/mongo-driver v1.5.1
	golang.org/x/crypto => github.com/golang/crypto v0.0.0-20210322153248-0c34fe9e7dc2
	golang.org/x/exp => github.com/golang/exp v0.0.0-20210220032938-85be41e4509f
	golang.org/x/image => github.com/golang/image v0.0.0-20210220032944-ac19c3e999fb
	golang.org/x/lint => github.com/golang/lint v0.0.0-20201208152925-83fdc39ff7b5
	golang.org/x/mobile => github.com/golang/mobile v0.0.0-20210220033013-bdb1ca9a1e08
	golang.org/x/mod => github.com/golang/mod v0.4.2
	golang.org/x/net => github.com/golang/net v0.0.0-20210330142815-c8897c278d10
	golang.org/x/oauth2 => github.com/golang/oauth2 v0.0.0-20210323180902-22b0adad7558
	golang.org/x/sync => github.com/golang/sync v0.0.0-20210220032951-036812b2e83c
	golang.org/x/sys => github.com/golang/sys v0.0.0-20220224120231-95c6836cb0e7
	golang.org/x/term => github.com/golang/term v0.0.0-20210317153231-de623e64d2a6
	golang.org/x/text => github.com/golang/text v0.3.5
	golang.org/x/time => github.com/golang/time v0.0.0-20210220033141-f8bda1e9f3ba
	golang.org/x/tools => github.com/golang/tools v0.1.0
	golang.org/x/xerrors => github.com/golang/xerrors v0.0.0-20200804184101-5ec99f83aff1
	google.golang.org/api => google.golang.org/api v0.43.0
	google.golang.org/genproto => google.golang.org/genproto v0.0.0-20210329143202-679c6ae281ee
	google.golang.org/grpc => google.golang.org/grpc v1.36.1
	gopkg.in/yaml.v2 => gopkg.in/yaml.v2 v2.4.0
)
