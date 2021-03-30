module pixielabs.ai/pixielabs

go 1.13

require (
	cloud.google.com/go/storage v1.10.0
	github.com/EvilSuperstars/go-cidrman v0.0.0-20190607145828-28e79e32899a
	github.com/Masterminds/sprig/v3 v3.2.2
	github.com/Microsoft/go-winio v0.4.16 // indirect
	github.com/PuerkitoBio/goquery v1.6.0
	github.com/ajstarks/svgo v0.0.0-20181006003313-6ce6a3bcf6cd // indirect
	github.com/alecthomas/chroma v0.7.1
	github.com/alecthomas/participle v0.4.1
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
	github.com/coreos/go-systemd/v22 v22.3.0 // indirect
	github.com/dgraph-io/badger/v3 v3.2011.1
	github.com/dgrijalva/jwt-go v3.2.0+incompatible
	github.com/docker/docker v17.12.0-ce-rc1.0.20200417080019-f432f7159508+incompatible
	github.com/dustin/go-humanize v1.0.0
	github.com/emicklei/dot v0.10.1
	github.com/evanphx/json-patch v4.9.0+incompatible
	github.com/fatih/color v1.7.0
	github.com/fogleman/gg v1.3.0 // indirect
	github.com/gdamore/tcell v1.3.0
	github.com/getsentry/sentry-go v0.5.1
	github.com/go-openapi/runtime v0.19.26
	github.com/go-openapi/strfmt v0.20.0
	github.com/gofrs/uuid v4.0.0+incompatible
	github.com/gogo/protobuf v1.3.2
	github.com/golang-migrate/migrate v3.5.4+incompatible
	github.com/golang/mock v1.5.0
	github.com/golang/protobuf v1.5.2 // indirect
	github.com/google/go-github/v32 v32.1.0
	github.com/google/uuid v1.2.0 // indirect
	github.com/googleapis/gnostic v0.2.0 // indirect
	github.com/googleapis/google-cloud-go-testing v0.0.0-20191008195207-8e1d251e947d
	github.com/gorilla/handlers v1.4.2
	github.com/gorilla/mux v1.7.4 // indirect
	github.com/gorilla/sessions v1.1.3
	github.com/graph-gophers/graphql-go v0.0.0-20190225005345-3e8838d4614c
	github.com/grpc-ecosystem/go-grpc-middleware v1.2.2
	github.com/huandu/xstrings v1.3.2 // indirect
	github.com/ianlancetaylor/cgosymbolizer v0.0.0-20200424224625-be1b05b0b279
	github.com/inconshreveable/go-update v0.0.0-20160112193335-8152e7eb6ccf
	github.com/jackc/fake v0.0.0-20150926172116-812a484cc733 // indirect
	github.com/jackc/pgx v3.5.0+incompatible
	github.com/jmoiron/sqlx v1.2.0
	github.com/kardianos/osext v0.0.0-20190222173326-2bc1f35cddc0
	github.com/kylelemons/godebug v1.1.0 // indirect
	github.com/lib/pq v1.3.0
	github.com/mattn/go-runewidth v0.0.9
	github.com/mitchellh/copystructure v1.1.1 // indirect
	github.com/moby/term v0.0.0-20201216013528-df9cb8a40635 // indirect
	github.com/morikuni/aec v1.0.0 // indirect
	github.com/nats-io/jwt v1.2.2 // indirect
	github.com/nats-io/nats-server/v2 v2.1.4
	github.com/nats-io/nats-streaming-server v0.17.0
	github.com/nats-io/nats.go v1.10.0
	github.com/nats-io/stan.go v0.8.2
	github.com/olekukonko/tablewriter v0.0.4
	github.com/olivere/elastic/v7 v7.0.12
	github.com/ory/dockertest/v3 v3.6.3
	github.com/ory/hydra-client-go v1.9.2
	github.com/ory/kratos-client-go v0.5.4-alpha.1
	github.com/phayes/freeport v0.0.0-20171002181615-b8543db493a5
	github.com/prometheus/client_golang v1.5.1
	github.com/rivo/tview v0.0.0-20200404204604-ca37f83cb2e7
	github.com/rivo/uniseg v0.1.0
	github.com/sahilm/fuzzy v0.1.0
	github.com/satori/go.uuid v1.2.0 // indirect
	github.com/segmentio/backo-go v0.0.0-20160424052352-204274ad699c // indirect
	github.com/sirupsen/logrus v1.7.0
	github.com/skratchdot/open-golang v0.0.0-20190402232053-79abb63cd66e
	github.com/spf13/cast v1.3.1
	github.com/spf13/cobra v1.1.1
	github.com/spf13/pflag v1.0.5
	github.com/spf13/viper v1.7.0
	github.com/stretchr/testify v1.6.1
	github.com/tidwall/buntdb v1.2.1 // indirect
	github.com/tidwall/rtree v0.0.0-20180113144539-6cd427091e0e // indirect
	github.com/txn2/txeh v1.2.1
	github.com/vbauerster/mpb/v4 v4.11.0
	github.com/xtgo/uuid v0.0.0-20140804021211-a0b114877d4c // indirect
	github.com/zenazn/goji v0.9.1-0.20160507202103-64eb34159fe5
	go.etcd.io/etcd/api/v3 v3.5.0-alpha.0
	go.etcd.io/etcd/client/v3 v3.5.0-alpha.0
	go.etcd.io/etcd/pkg/v3 v3.5.0-alpha.0
	go.uber.org/multierr v1.6.0 // indirect
	golang.org/x/crypto v0.0.0-20210220033148-5ea612d1eb83 // indirect
	golang.org/x/mod v0.4.2 // indirect
	golang.org/x/net v0.0.0-20210330142815-c8897c278d10
	golang.org/x/oauth2 v0.0.0-20210313182246-cd4f82c27b84
	golang.org/x/sync v0.0.0-20210220032951-036812b2e83c
	golang.org/x/sys v0.0.0-20210326220804-49726bf1d181
	gonum.org/v1/gonum v0.0.0-20190413104459-5d695651a1d5 // indirect
	gonum.org/v1/plot v0.0.0-20190410204940-3a5f52653745
	google.golang.org/api v0.43.0
	google.golang.org/genproto v0.0.0-20210329143202-679c6ae281ee
	google.golang.org/grpc v1.36.1
	gopkg.in/segmentio/analytics-go.v3 v3.1.0
	gopkg.in/src-d/go-git.v4 v4.13.1
	gopkg.in/yaml.v2 v2.4.0
	k8s.io/api v0.18.2
	k8s.io/apimachinery v0.18.2
	k8s.io/cli-runtime v0.18.2
	k8s.io/client-go v0.18.2
	k8s.io/klog v1.0.0
	k8s.io/kube-openapi v0.0.0-20200204173128-addea2498afe // indirect
	k8s.io/kubectl v0.18.2
	sigs.k8s.io/yaml v1.2.0
)

replace (
	// Fix go mod issues with upper case Sirupsen.
	github.com/Sirupsen/logrus v1.7.0 => github.com/sirupsen/logrus v1.7.0
	github.com/coreos/etcd => go.etcd.io/etcd/v3 v3.5.0-alpha.0
	github.com/go-openapi/analysis => github.com/go-openapi/analysis v0.19.5
	github.com/go-openapi/loads => github.com/go-openapi/loads v0.19.0
	github.com/go-openapi/spec => github.com/go-openapi/spec v0.19.3
)
