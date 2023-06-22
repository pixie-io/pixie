# Golang GRPC client-server using Pixie Services Framework (golang)

Pixie's own services framework mixes the use of Golang's HTTP & HTTP2 APIs, which represent
a widely used library usage pattern that should be supported by Pixie. Therefore this directory has
a `_pl` suffix.

These client and server rules are built against different versions of Golang releases as well,
in order for tests to verify Stirling against different Golang versions.

You can use skaffold to run these demos:

```shell
kubectl create ns px-grpc-test
skaffold run -f src/stirling/testing/demo_apps/go_grpc_tls_pl/skaffold.yaml
```
