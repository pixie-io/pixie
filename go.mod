module px.dev/pixie

go 1.16

require (
	cloud.google.com/go/storage v1.10.0
	github.com/Azure/go-autorest/autorest v0.11.18 // indirect
	github.com/EvilSuperstars/go-cidrman v0.0.0-20190607145828-28e79e32899a
	github.com/Masterminds/sprig/v3 v3.2.2
	github.com/Microsoft/go-winio v0.4.16 // indirect
	github.com/PuerkitoBio/goquery v1.6.0
	github.com/alecthomas/chroma v0.7.1
	github.com/alecthomas/participle v0.4.1
	github.com/armon/go-metrics v0.3.6 // indirect
	github.com/badoux/checkmail v0.0.0-20181210160741-9661bd69e9ad
	github.com/bazelbuild/rules_go v0.22.4
	github.com/blang/semver v3.5.1+incompatible
	github.com/bmatcuk/doublestar v1.2.2
	github.com/bmizerany/assert v0.0.0-20160611221934-b7ed37b82869 // indirect
	github.com/cenkalti/backoff/v3 v3.2.2
	github.com/cockroachdb/apd v1.1.0 // indirect
	github.com/cockroachdb/pebble v0.0.0-20210120202502-6110b03a8a85
	github.com/containerd/containerd v1.3.4 // indirect
	github.com/containerd/continuity v0.0.0-20210208174643-50096c924a4e // indirect
	github.com/dgraph-io/badger/v3 v3.2011.1
	github.com/dgrijalva/jwt-go/v4 v4.0.0-preview1
	github.com/docker/docker v20.10.5+incompatible // indirect
	github.com/dustin/go-humanize v1.0.0
	github.com/emicklei/dot v0.10.1
	github.com/evanphx/json-patch v4.9.0+incompatible
	github.com/fatih/color v1.10.0
	github.com/gdamore/tcell v1.3.0
	github.com/getsentry/sentry-go v0.5.1
	github.com/go-openapi/runtime v0.19.26
	github.com/go-openapi/strfmt v0.20.0
	github.com/go-sql-driver/mysql v1.6.0 // indirect
	github.com/gofrs/uuid v4.0.0+incompatible
	github.com/gogo/protobuf v1.3.2
	github.com/golang-migrate/migrate v3.5.4+incompatible
	github.com/golang/mock v1.5.0
	github.com/google/go-github/v32 v32.1.0
	github.com/google/gofuzz v1.2.0 // indirect
	github.com/google/uuid v1.2.0 // indirect
	github.com/googleapis/google-cloud-go-testing v0.0.0-20191008195207-8e1d251e947d
	github.com/gorilla/handlers v1.5.1
	github.com/gorilla/sessions v1.2.1
	github.com/graph-gophers/graphql-go v1.1.0
	github.com/grpc-ecosystem/go-grpc-middleware v1.2.2
	github.com/hashicorp/go-hclog v0.15.0 // indirect
	github.com/hashicorp/go-immutable-radix v1.3.0 // indirect
	github.com/huandu/xstrings v1.3.2 // indirect
	github.com/ianlancetaylor/cgosymbolizer v0.0.0-20200424224625-be1b05b0b279
	github.com/inconshreveable/go-update v0.0.0-20160112193335-8152e7eb6ccf
	github.com/jackc/fake v0.0.0-20150926172116-812a484cc733 // indirect
	github.com/jackc/pgx v3.5.0+incompatible
	github.com/jmoiron/sqlx v1.2.0
	github.com/kardianos/osext v0.0.0-20190222173326-2bc1f35cddc0
	github.com/klauspost/compress v1.11.13 // indirect
	github.com/kylelemons/godebug v1.1.0 // indirect
	github.com/lib/pq v1.10.0
	github.com/mattn/go-runewidth v0.0.9
	github.com/minio/highwayhash v1.0.2 // indirect
	github.com/mitchellh/copystructure v1.1.1 // indirect
	github.com/moby/term v0.0.0-20201216013528-df9cb8a40635 // indirect
	github.com/nats-io/nats-server/v2 v2.2.0
	github.com/nats-io/nats-streaming-server v0.21.1
	github.com/nats-io/nats.go v1.10.1-0.20210228004050-ed743748acac
	github.com/nats-io/stan.go v0.8.3
	github.com/olekukonko/tablewriter v0.0.5
	github.com/olivere/elastic/v7 v7.0.12
	github.com/ory/dockertest/v3 v3.6.3
	github.com/ory/hydra-client-go v1.9.2
	github.com/ory/kratos-client-go v0.5.4-alpha.1
	github.com/phayes/freeport v0.0.0-20171002181615-b8543db493a5
	github.com/rivo/tview v0.0.0-20200404204604-ca37f83cb2e7
	github.com/rivo/uniseg v0.1.0
	github.com/sahilm/fuzzy v0.1.0
	github.com/satori/go.uuid v1.2.0 // indirect
	github.com/segmentio/backo-go v0.0.0-20160424052352-204274ad699c // indirect
	github.com/sercand/kuberesolver/v3 v3.0.0
	github.com/sirupsen/logrus v1.7.0
	github.com/skratchdot/open-golang v0.0.0-20190402232053-79abb63cd66e
	github.com/spf13/cast v1.3.1
	github.com/spf13/cobra v1.2.1
	github.com/spf13/pflag v1.0.5
	github.com/spf13/viper v1.8.1
	github.com/stretchr/testify v1.7.0
	github.com/tidwall/buntdb v1.2.1
	github.com/txn2/txeh v1.2.1
	github.com/vbauerster/mpb/v4 v4.11.0
	github.com/xtgo/uuid v0.0.0-20140804021211-a0b114877d4c // indirect
	github.com/zenazn/goji v0.9.1-0.20160507202103-64eb34159fe5
	go.etcd.io/etcd/api/v3 v3.5.0
	go.etcd.io/etcd/client/pkg/v3 v3.5.0
	go.etcd.io/etcd/client/v3 v3.5.0
	go.mongodb.org/mongo-driver v1.5.0 // indirect
	golang.org/x/crypto v0.0.0-20210322153248-0c34fe9e7dc2 // indirect
	golang.org/x/mod v0.4.2 // indirect
	golang.org/x/net v0.0.0-20210330230544-e57232859fb2
	golang.org/x/oauth2 v0.0.0-20210313182246-cd4f82c27b84
	golang.org/x/sync v0.0.0-20210220032951-036812b2e83c
	golang.org/x/sys v0.0.0-20210611083646-a4fc73990273
	golang.org/x/term v0.0.0-20210317153231-de623e64d2a6 // indirect
	golang.org/x/time v0.0.0-20210220033141-f8bda1e9f3ba // indirect
	golang.org/x/tools v0.1.3 // indirect
	google.golang.org/api v0.44.0
	google.golang.org/genproto v0.0.0-20210602131652-f16073e35f0c
	google.golang.org/grpc v1.38.0
	google.golang.org/grpc/examples v0.0.0-20210326170912-4a19753e9dfd // indirect
	gopkg.in/segmentio/analytics-go.v3 v3.1.0
	gopkg.in/src-d/go-git.v4 v4.13.1
	gopkg.in/yaml.v2 v2.4.0
	k8s.io/api v0.20.5
	k8s.io/apimachinery v0.20.5
	k8s.io/cli-runtime v0.20.5
	k8s.io/client-go v0.20.5
	k8s.io/klog/v2 v2.8.0
	k8s.io/kube-openapi v0.0.0-20210305001622-591a79e4bda7 // indirect
	k8s.io/kubectl v0.20.5
	k8s.io/utils v0.0.0-20210305010621-2afb4311ab10 // indirect
	sigs.k8s.io/controller-runtime v0.8.3
	sigs.k8s.io/structured-merge-diff/v4 v4.1.0 // indirect
	sigs.k8s.io/yaml v1.2.0
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
	github.com/nats-io/nats-server/v2 => github.com/nats-io/nats-server/v2 v2.2.0
	github.com/nats-io/nats.go => github.com/nats-io/nats.go v1.10.0
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
	golang.org/x/sys => github.com/golang/sys v0.0.0-20210326220804-49726bf1d181
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
