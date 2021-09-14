# HTTP2/GRPC client-server using Pixie Services Framework (golang)

To run, first build everything:
```
bazel build //src/stirling/testing/demo_apps/go_grpc_tls_pl/...
```

Then execute the following commands in two separate terminals:

```
${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_grpc_tls_pl/server/server_/server \
  --server_tls_cert=${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_grpc_tls_pl/certs/server.crt \
  --server_tls_key=${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_grpc_tls_pl/certs/server.key \
  --tls_ca_cert=${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_grpc_tls_pl/certs/ca.crt
```

```
${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_grpc_tls_pl/client/client_/client \
  --count 10 \
  --client_tls_cert=${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_grpc_tls_pl/certs/client.crt \
  --client_tls_key=${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_grpc_tls_pl/certs/client.key \
  --tls_ca_cert=${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_grpc_tls_pl/certs/ca.crt
```
\
