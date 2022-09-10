# Golang GRPC client-server using Pixie Services Framework (golang)

Pixie's own services framework mixes the use of Golang's HTTP & HTTP2 APIs, which represent
a widely used library usage pattern that should be supported by Pixie. Therefore this directory has
a `_pl` suffix.

These client and server rules are built against different versions of Golang releases as well,
in order for tests to verify Stirling against different Golang versions.

To run, first build everything:
```
bazel run //src/stirling/testing/demo_apps/go_grpc_tls_pl/server:golang_1_16_grpc_tls_server -- --norun
bazel run //src/stirling/testing/demo_apps/go_grpc_tls_pl/client:golang_1_16_grpc_tls_client -- --norun
```

Then execute the following commands in two separate terminals:

```
docker run --name=grpc_tls_server bazel/src/stirling/testing/demo_apps/go_grpc_tls_pl/server:golang_1_16_grpc_tls_server
```

```
docker run --name=grpc_tls_client --network=container:grpc_tls_server bazel/src/stirling/testing/demo_apps/go_grpc_tls_pl/client:golang_1_16_grpc_tls_client
```
