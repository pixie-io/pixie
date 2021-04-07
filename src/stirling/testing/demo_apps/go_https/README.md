# HTTP server

To run, first build everything:
```
bazel build //src/stirling/testing/demo_apps/go_https/...
```

Then execute the following commands in two separate terminals:

```
${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_https/server/https_server_/https_server \
  --cert=${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_https/server/server.crt \
  --key=${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_https/server/server.key
```

```
${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_https/client/https_client_/https_client \
  --iters 3 --sub_iters 3
```
