# HTTP2/GRPC client-server using Pixie Services Framework (golang)

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
