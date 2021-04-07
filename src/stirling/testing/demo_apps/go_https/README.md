# HTTP server

To run, execute the following commands in two separate terminals:

```
cd ${PIXIE_ROOT}/src/stirling/testing/demo_apps/go_https/server
go build https_server.go
cd ${PIXIE_ROOT}
./src/stirling/testing/demo_apps/go_https/server/https_server
```

```
cd ${PIXIE_ROOT}/src/stirling/testing/demo_apps/go_https/client
go build https_client.go
cd ${PIXIE_ROOT}
./src/stirling/testing/demo_apps/go_https/client/https_client
```

