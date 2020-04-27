# HTTP2/GRPC client-server (golang)

To run, execute the following commands in two separate terminals:

```
cd ${PIXIE_ROOT}/demos/client_server_apps/go_grpc_tls/server
go build server.go
./server
```

```
cd ${PIXIE_ROOT}/demos/client_server_apps/go_grpc_tls/client
go build client.go
./client
```

Note that we avoid using bazel run in this case, because the bazel build doesn't always include debug symbols,
which are required for Uprobe-based tracing purposes.

