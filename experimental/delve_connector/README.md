# Delve Custom Front-End Client

### Step 1: Run a server as a tracing target.
```
bazel build -c opt //src/stirling/testing/demo_apps/go_grpc_tls/server:server
${PIXIE_ROOT}/bazel-bin/src/stirling/testing/demo_apps/go_grpc_tls/server/linux_amd64/server
```

### Step 2A: Run Delve backend.
```
sudo dlv --headless --log --log-output=rpc --listen=127.0.0.1:44101 attach $(pgrep server)
```

### Step 2B: Run Delve front-end.
```
bazel run //experimental/delve_connector:client -- --port=44101
# Add --quiet=1 for benchmarking
```

For comparsion, one could also run the native delve front-end.
```
dlv connect localhost:44101
(dlv) t golang.org/x/net/http2.(*Framer).WriteDataPadded
(dlv) continue
```

### Step 3: Run client

```
bazel build -c opt //src/stirling/testing/demo_apps/go_grpc_tls/client:client
$PIXIE_ROOT/bazel-bin/src/stirling/testing/demo_apps/go_grpc_tls/client/linux_amd64/client --benchmark --rounds=3
```

### More Notes

For benchmarking, one may want to disable turbo boost
```
echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
```

Step 2 could be replaced with a call to stirling_wrapper to benchmark stirling overheads.
Note that Stirling attaches multiple uprobes, so the comparison will not be fair unless the extra
probes are removed.
