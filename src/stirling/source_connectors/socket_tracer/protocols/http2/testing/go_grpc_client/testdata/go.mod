module pxgrpc

go 1.23.0

toolchain go1.23.6

require (
	google.golang.org/grpc v1.53.0
	px.dev/pixie v0.0.0-20250317143437-0442252dcb4e
)

require (
	github.com/gogo/protobuf v1.3.2 // indirect
	github.com/golang/protobuf v1.5.3 // indirect
	golang.org/x/net v0.36.0 // indirect
	golang.org/x/sys v0.30.0 // indirect
	golang.org/x/text v0.22.0 // indirect
	google.golang.org/genproto v0.0.0-20230306155012-7f2fa6fef1f4 // indirect
	google.golang.org/protobuf v1.29.1 // indirect
)

replace (
	cloud.google.com/go => cloud.google.com/go v0.80.0
	github.com/Azure/go-ansiterm => github.com/Azure/go-ansiterm v0.0.0-20170929234023-d6e3b3328b78
	github.com/Azure/go-autorest => github.com/Azure/go-autorest v14.2.0+incompatible
	github.com/Azure/go-autorest/autorest => github.com/Azure/go-autorest/autorest v0.11.1
	github.com/Sirupsen/logrus => github.com/sirupsen/logrus v1.7.0
	github.com/docker/docker => github.com/moby/moby v23.0.5+incompatible
	github.com/go-openapi/analysis => github.com/go-openapi/analysis v0.19.5
	github.com/go-openapi/loads => github.com/go-openapi/loads v0.19.0
	github.com/go-openapi/spec => github.com/go-openapi/spec v0.19.3
	github.com/go-openapi/strfmt => github.com/go-openapi/strfmt v0.20.0
	// Upgrade after https://github.com/golang/mock/pull/601 makes it into a release
	github.com/golang/mock => github.com/golang/mock v1.5.0
	github.com/golang/protobuf => github.com/golang/protobuf v1.5.2
	github.com/google/go-cmp => github.com/google/go-cmp v0.5.5
	google.golang.org/api => google.golang.org/api v0.43.0
	google.golang.org/genproto => google.golang.org/genproto v0.0.0-20211208223120-3a66f561d7aa
	gopkg.in/yaml.v2 => gopkg.in/yaml.v2 v2.4.0
)
